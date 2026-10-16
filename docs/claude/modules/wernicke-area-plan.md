# Wernicke's Area Integration Plan

**Version**: 2.0.0
**Created**: 2026-01-04
**Updated**: 2026-01-04
**Status**: Proposed (Comprehensive Integration)

### Integration Summary
| Category | Module Count |
|----------|--------------|
| GPU Kernels | 771+ across 57 CUDA files |
| Quantum Algorithms | 5 modules (QMC, QMCTS, Walks, Reasoning, Amplitude) |
| Math Utilities | 7 modules (FFT, Complex, Tensor, ODE, Filter, Stats, LinAlg) |
| Brain Regions | 15 regions |
| Hypothalamus | 5 sub-modules |
| Omni Inference | 7 modules |
| Logic/Reasoning | 6 modules |
| **Total Files** | ~30 headers, ~30 sources, 4 CUDA kernels |

---

## Overview

Wernicke's area (posterior superior temporal gyrus, Brodmann area 22) is critical for language comprehension. This document outlines the integration plan for implementing Wernicke's area within the NIMCP architecture.

### Neurobiological Basis

| Aspect | Description |
|--------|-------------|
| **Location** | Posterior superior temporal gyrus (BA 22), typically left hemisphere |
| **Function** | Language comprehension, semantic processing, phoneme categorization |
| **Damage Result** | Wernicke's aphasia: fluent but nonsensical speech, poor comprehension |
| **Key Connection** | Arcuate fasciculus to Broca's area (bidirectional) |

---

## Existing Relevant Modules

| Module | Location | Wernicke Integration Point |
|--------|----------|---------------------------|
| **Speech Cortex** | `include/perception/nimcp_speech_cortex.h` | **Partial Wernicke's** - phoneme recognition, formant tracking |
| **Temporal Lobe Adapter** | `include/core/brain/regions/temporal/nimcp_temporal_adapter.h` | Auditory processing, semantic memory |
| **Broca's Area** | `include/core/brain/regions/broca/` | Language production (arcuate fasciculus connection) |
| **Audio Cortex** | `include/perception/nimcp_audio_cortex.h` | Primary auditory processing (A1) |
| **Cochlea** | `include/perception/nimcp_cochlea.h` | Peripheral auditory input |
| **Semantic Memory** | `include/cognitive/memory/nimcp_semantic_memory.h` | Concept retrieval, spreading activation |
| **Knowledge Graph** | `include/core/brain/nimcp_brain_kg.h` | Module registration, semantic edges |
| **Omni Sensory Bridge** | `include/perception/nimcp_omni_sensory_bridge.h` | Cross-modal prediction |
| **Omni Broca Bridge** | `include/core/brain/regions/broca/nimcp_omni_broca_bridge.h` | Language production prediction |
| **Cortical Columns** | `include/core/cortical_columns/` | Hierarchical predictive coding |
| **FEP System** | `include/cognitive/free_energy/nimcp_free_energy.h` | Prediction error minimization |
| **Audiovisual Bridge** | `include/core/brain/regions/occipital/nimcp_occipital_audiovisual_bridge.h` | Lip reading, McGurk effect |

---

## Integration Architecture

```
                                    ┌─────────────────────────────────────┐
                                    │         PREFRONTAL CORTEX           │
                                    │    (Executive, Context, Attention)   │
                                    └──────────────┬──────────────────────┘
                                                   │ Top-down context
                                                   ▼
┌──────────────────┐              ┌─────────────────────────────────────────┐
│   COCHLEA        │──────────────▶│            WERNICKE'S AREA              │
│ (Peripheral)     │   Tonotopic   │         (Posterior STG/BA22)            │
└──────────────────┘   Input       │                                         │
         │                         │  ┌─────────────────────────────────┐    │
         ▼                         │  │  Phonological Analysis Layer    │    │
┌──────────────────┐              │  │  - Phoneme categorization       │    │
│  AUDIO CORTEX    │──────────────▶│  │  - Formant tracking (F1-F4)    │    │
│  (A1/BA41-42)    │   Spectral    │  │  - Prosody extraction          │    │
└──────────────────┘   Features    │  └─────────────┬───────────────────┘    │
                                    │                │                        │
┌──────────────────┐              │  ┌─────────────▼───────────────────┐    │
│ OCCIPITAL/       │──────────────▶│  │  Lexical Access Layer          │    │
│ AUDIOVISUAL      │   Lip shapes  │  │  - Word recognition            │    │
│ BRIDGE           │   McGurk      │  │  - Lexicon lookup (10k words)  │    │
└──────────────────┘              │  │  - Frequency effects            │    │
                                    │  └─────────────┬───────────────────┘    │
                                    │                │                        │
                                    │  ┌─────────────▼───────────────────┐    │
                                    │  │  Semantic Integration Layer     │◀───┼──── SEMANTIC MEMORY
                                    │  │  - Concept activation           │    │     (Spreading activation)
                                    │  │  - Context integration          │    │
                                    │  │  - Disambiguation               │    │
                                    │  └─────────────┬───────────────────┘    │
                                    │                │                        │
                                    │  ┌─────────────▼───────────────────┐    │
                                    │  │  Syntactic Comprehension Layer  │    │
                                    │  │  - Phrase structure parsing     │    │
                                    │  │  - Thematic role assignment     │    │
                                    │  │  - Sentence-level integration   │    │
                                    │  └─────────────────────────────────┘    │
                                    └──────────────┬──────────────────────────┘
                                                   │
                        ┌──────────────────────────┼──────────────────────────┐
                        │                          │                          │
                        ▼                          ▼                          ▼
          ┌─────────────────────┐    ┌─────────────────────┐    ┌─────────────────────┐
          │    BROCA'S AREA     │    │   KNOWLEDGE GRAPH   │    │  WORKING MEMORY     │
          │  (Language Output)  │    │ (Semantic Network)  │    │ (Phonological Loop) │
          │                     │    │                     │    │                     │
          │ Arcuate Fasciculus  │◀───│  Concept Relations  │    │  7±2 items buffer   │
          │ (bidirectional)     │    │  Spreading Activation│    │  Rehearsal loop     │
          └─────────────────────┘    └─────────────────────┘    └─────────────────────┘
```

---

## Key Integration Points

### 1. Auditory Input Pipeline

```c
// Existing flow to enhance:
cochlea → audio_cortex → speech_cortex (partial Wernicke's)

// New flow:
cochlea → audio_cortex → wernicke_adapter → {semantic_memory, broca, kg}
```

### 2. Broca-Wernicke Connection (Arcuate Fasciculus)

```c
// Language Production Bridge already exists:
lpb_repeat_last_heard()     // Wernicke → Broca (repetition)
lpb_produce_from_intent()   // Semantic → Broca (production)

// Need to add:
wernicke_send_comprehension_to_broca()  // Forward comprehension
broca_send_efference_copy_to_wernicke() // Self-monitoring feedback
```

### 3. Audiovisual Integration (McGurk Effect)

```c
// Existing in occipital_audiovisual_bridge:
visual_speech_observation_t  // Lip shapes, gestures
av_binding_event_t           // Cross-modal binding

// Wernicke receives:
- Audio phonemes from speech_cortex
- Visual phonemes from audiovisual_bridge
- Fused percept via McGurk processing
```

### 4. Semantic Memory Integration

```c
// Existing semantic memory API:
semantic_memory_activate()           // Activate concept
semantic_memory_get_related()        // Spreading activation
semantic_memory_query()              // Concept retrieval

// Wernicke needs:
- Word → Concept mapping (lexical-semantic interface)
- Context-dependent disambiguation
- Priming from prior context
```

### 5. Cortical Column Integration

```c
// Existing predictive coding:
cortical_predictive_compute_prediction()  // Top-down
cortical_predictive_compute_error()       // Bottom-up PE

// Wernicke cortical organization:
- L2/3: Prediction error (phoneme/word mismatch)
- L5/6: Top-down predictions (expected phonemes)
- Hierarchical: Phoneme → Syllable → Word → Phrase
```

### 6. Omnidirectional Inference

```c
// Existing omni bridges:
omni_sensory_bridge   // Audio/Visual/Speech modalities
omni_broca_bridge     // Language production

// New:
omni_wernicke_bridge  // Language comprehension
  - Forward: Predict next phoneme/word
  - Backward: Infer meaning from context
  - Lateral: Cross-modal speech perception
```

### 7. Knowledge Graph Registration

```c
// Module registration pattern:
brain_kg_add_node(kg, "Wernicke_Area", BRAIN_KG_NODE_CORTICAL, ...);
brain_kg_add_edge(kg, wernicke_id, broca_id, BRAIN_KG_EDGE_CONNECTS_TO, ...);
brain_kg_add_edge(kg, wernicke_id, semantic_id, BRAIN_KG_EDGE_INTEGRATES_WITH, ...);

// Omni KG sync:
omni_kg_register_module(sync, "wernicke", OMNI_KG_TYPE_LANGUAGE, caps);
omni_kg_add_bidirectional_edge(sync, wernicke_id, broca_id, precision);
```

---

## Proposed Wernicke's Area Architecture

### Header Structure (`nimcp_wernicke_adapter.h`)

```c
// Core components (4 processing layers)
typedef struct {
    phonological_analyzer_t* phonological;    // Phoneme → syllable
    lexical_access_t* lexical;                // Word recognition
    semantic_integrator_t* semantic;          // Meaning integration
    syntactic_parser_t* syntactic;            // Sentence parsing
} wernicke_components_t;

// Connection points
typedef struct {
    audio_cortex_t* audio_cortex;             // A1 input
    speech_cortex_t* speech_cortex;           // Phoneme input
    broca_adapter_t* broca;                   // Production output
    semantic_memory_t* semantic_memory;       // Concept store
    occipital_audiovisual_bridge_t* av_bridge; // Lip reading
    brain_kg_t* kg;                           // Knowledge graph
} wernicke_connections_t;

// Main adapter
typedef struct wernicke_adapter wernicke_adapter_t;
```

### Key APIs

```c
// Lifecycle
wernicke_adapter_t* wernicke_create(const wernicke_config_t* config);
void wernicke_destroy(wernicke_adapter_t* wernicke);

// Core comprehension
wernicke_result_t wernicke_process_audio(wernicke_adapter_t* w,
                                          const float* audio, uint32_t samples);
wernicke_result_t wernicke_process_phonemes(wernicke_adapter_t* w,
                                             const phoneme_event_t* phonemes, uint32_t count);

// Word recognition
int wernicke_recognize_word(wernicke_adapter_t* w,
                            const phoneme_t* phonemes, uint32_t count,
                            wernicke_word_t* result);

// Semantic integration
int wernicke_get_meaning(wernicke_adapter_t* w,
                         const char* word, wernicke_concept_t* concept);
int wernicke_disambiguate(wernicke_adapter_t* w,
                          const char* word, const wernicke_context_t* ctx,
                          wernicke_concept_t* concept);

// Sentence comprehension
int wernicke_parse_sentence(wernicke_adapter_t* w,
                            const wernicke_word_t* words, uint32_t count,
                            wernicke_parse_t* parse);

// Cross-modal integration
int wernicke_integrate_audiovisual(wernicke_adapter_t* w,
                                   const phoneme_event_t* audio_phonemes,
                                   const visual_speech_observation_t* visual,
                                   phoneme_event_t* fused);

// Broca connection (arcuate fasciculus)
int wernicke_send_to_broca(wernicke_adapter_t* w,
                           const wernicke_comprehension_t* comprehension);
int wernicke_receive_efference_copy(wernicke_adapter_t* w,
                                    const broca_efference_copy_t* efference);

// Prediction (for omni bridge)
int wernicke_predict_next_phoneme(wernicke_adapter_t* w, float* prediction);
int wernicke_predict_next_word(wernicke_adapter_t* w, wernicke_word_pred_t* pred);

// Working memory
int wernicke_wm_store(wernicke_adapter_t* w, const phoneme_t* phonemes, uint32_t count);
int wernicke_wm_rehearse(wernicke_adapter_t* w);
int wernicke_wm_get_contents(wernicke_adapter_t* w, phoneme_t* buffer, uint32_t* count);
```

---

## Proposed File Structure

```
include/core/brain/regions/wernicke/
├── nimcp_wernicke_adapter.h          # Main adapter
├── nimcp_phonological_analyzer.h     # Phoneme/syllable processing
├── nimcp_lexical_access.h            # Word recognition
├── nimcp_semantic_integrator.h       # Meaning integration
├── nimcp_syntactic_comprehension.h   # Sentence parsing
├── nimcp_wernicke_thalamic_bridge.h  # MGN relay
├── nimcp_wernicke_substrate_bridge.h # Bio-async
├── nimcp_wernicke_quantum_bridge.h   # Quantum acceleration
├── nimcp_omni_wernicke_bridge.h      # Omnidirectional inference
├── nimcp_wernicke_broca_bridge.h     # Arcuate fasciculus
└── nimcp_wernicke_immune.h           # Immune integration

src/core/brain/regions/wernicke/
├── nimcp_wernicke_adapter.c
├── nimcp_phonological_analyzer.c
├── nimcp_lexical_access.c
├── nimcp_semantic_integrator.c
├── nimcp_syntactic_comprehension.c
└── ... (bridge implementations)
```

---

## Data Flow Summary

| Source | → Wernicke Layer | → Destination |
|--------|------------------|---------------|
| Cochlea/Audio Cortex | Phonological | Working Memory |
| Speech Cortex | Phonological | Lexical Access |
| Audiovisual Bridge | Phonological (McGurk) | Lexical Access |
| Lexical Access | Semantic | Semantic Memory |
| Semantic Memory | Semantic | Knowledge Graph |
| Syntactic Parser | Comprehension | Broca's Area |
| Broca's Efference | Self-Monitor | Error Correction |

---

## Implementation Phases

### Phase 1: Core Adapter
- `nimcp_wernicke_adapter.h/.c` - Main lifecycle and configuration
- Basic phoneme processing pipeline
- Integration with existing speech_cortex

### Phase 2: Processing Layers
- `nimcp_phonological_analyzer.h/.c` - Phoneme categorization
- `nimcp_lexical_access.h/.c` - Word recognition with lexicon
- `nimcp_semantic_integrator.h/.c` - Concept activation

### Phase 3: Bridges
- `nimcp_wernicke_broca_bridge.h/.c` - Arcuate fasciculus
- `nimcp_wernicke_thalamic_bridge.h/.c` - MGN relay
- `nimcp_wernicke_substrate_bridge.h/.c` - Bio-async messaging

### Phase 4: Omnidirectional Integration
- `nimcp_omni_wernicke_bridge.h/.c` - Predictive comprehension
- KG sync for self-awareness
- Cross-modal prediction (audiovisual)

### Phase 5: Advanced Features
- Syntactic comprehension
- Context-dependent disambiguation
- Self-monitoring via Broca feedback
- Quantum acceleration

---

## Dependencies

### Required Modules
- `nimcp_speech_cortex` - Phoneme input
- `nimcp_audio_cortex` - Spectral features
- `nimcp_semantic_memory` - Concept storage
- `nimcp_brain_kg` - Module registration
- `nimcp_bio_router` - Message passing

### Optional Enhancements
- `nimcp_broca_adapter` - Full language loop
- `nimcp_occipital_audiovisual_bridge` - Lip reading
- `nimcp_omni_*` - Omnidirectional inference
- `nimcp_cortical_columns` - Predictive coding

---

## Testing Strategy

### Unit Tests
- Phoneme categorization accuracy
- Word recognition with noise
- Semantic disambiguation

### Integration Tests
- Audio → Comprehension pipeline
- Broca-Wernicke bidirectional communication
- Audiovisual fusion (McGurk)

### Regression Tests
- Latency benchmarks
- Memory footprint
- Comprehension accuracy metrics

---

## Comprehensive Module Integration

### GPU Acceleration Integration

Wernicke's area processing can leverage the extensive GPU infrastructure (57 CUDA files, 771+ kernels):

| GPU Module | Integration Purpose | Header |
|------------|---------------------|--------|
| **Tensor Operations** | Phoneme embedding matrices, attention weights | `include/gpu/cuda/tensor/nimcp_cuda_tensor_ops.cuh` |
| **Flash Attention** | Self-attention for contextual word embeddings | `include/gpu/cuda/nimcp_cuda_flash_attention.cuh` |
| **Matrix Multiply** | Lexicon lookup, semantic projections | `include/gpu/cuda/nimcp_cuda_matmul.cuh` |
| **FFT Operations** | Spectral analysis for formant extraction | `include/gpu/cuda/nimcp_cuda_fft.cuh` |
| **Softmax** | Word probability distributions | `include/gpu/cuda/nimcp_cuda_softmax.cuh` |
| **Batch Norm** | Normalization layers in neural components | `include/gpu/cuda/nimcp_cuda_batch_norm.cuh` |
| **Mixed Precision** | FP16/BF16 for efficient embedding operations | `include/gpu/cuda/nimcp_cuda_mixed_precision.cuh` |
| **Multi-GPU** | Distributed lexicon for large vocabularies | `include/gpu/cuda/nimcp_cuda_multi_gpu.cuh` |

```c
// GPU-accelerated phoneme embedding
typedef struct {
    nimcp_cuda_tensor_t* phoneme_embeddings;  // [num_phonemes x embed_dim]
    nimcp_cuda_tensor_t* word_embeddings;      // [vocab_size x embed_dim]
    nimcp_cuda_attention_t* self_attention;    // Contextual processing
    cudaStream_t stream;
} wernicke_gpu_ctx_t;

// Key GPU APIs for Wernicke
wernicke_gpu_ctx_t* wernicke_gpu_create(const wernicke_gpu_config_t* config);
int wernicke_gpu_embed_phonemes(wernicke_gpu_ctx_t* ctx,
                                 const phoneme_t* phonemes, uint32_t count,
                                 float* embeddings);
int wernicke_gpu_lexicon_lookup(wernicke_gpu_ctx_t* ctx,
                                 const float* phoneme_sequence,
                                 wernicke_word_t* candidates, uint32_t* n_candidates);
```

---

### Quantum Algorithm Integration

Wernicke's area can leverage quantum algorithms for semantic search and disambiguation:

| Quantum Module | Integration Purpose | Header |
|----------------|---------------------|--------|
| **Quantum Monte Carlo** | Semantic spreading activation sampling | `include/quantum/nimcp_qmc.h` |
| **QMCTS** | Sentence parsing with quantum walk guidance | `include/quantum/nimcp_qmcts.h` |
| **Quantum Walks** | O(√N) semantic neighborhood search | `include/quantum/nimcp_quantum_walk.h` |
| **Quantum Reasoning** | Disambiguation via Grover's search | `include/quantum/nimcp_quantum_reasoning.h` |
| **Amplitude Estimation** | Probability estimation for word candidates | `include/quantum/nimcp_amplitude_estimation.h` |

```c
// Quantum-accelerated semantic search
typedef struct {
    qmc_context_t* qmc;                    // Monte Carlo sampling
    qmcts_context_t* qmcts;                // Tree search for parsing
    quantum_walk_t* semantic_walk;         // Spreading activation
    quantum_reasoning_ctx_t* reasoning;    // Disambiguation
} wernicke_quantum_ctx_t;

// Quantum APIs for Wernicke
int wernicke_quantum_semantic_search(wernicke_quantum_ctx_t* ctx,
                                      const char* word,
                                      semantic_concept_t* related,
                                      uint32_t max_results);
int wernicke_quantum_disambiguate(wernicke_quantum_ctx_t* ctx,
                                   const char* ambiguous_word,
                                   const wernicke_context_t* context,
                                   semantic_concept_t* resolved);
int wernicke_quantum_parse_tree(wernicke_quantum_ctx_t* ctx,
                                 const wernicke_word_t* words, uint32_t count,
                                 parse_tree_t* tree);
```

---

### Math Utilities Integration

Wernicke's area integrates with core math utilities for signal processing and neural computation:

| Math Module | Integration Purpose | Header |
|-------------|---------------------|--------|
| **Complex Math** | Phase-amplitude coupling for neural oscillations | `include/utils/math/nimcp_complex.h` |
| **FFT** | Spectral decomposition for formant analysis | `include/utils/math/nimcp_fft.h` |
| **Tensor Ops** | Neural network forward/backward passes | `include/utils/math/nimcp_tensor.h` |
| **ODE Integration** | Neural dynamics (Euler, RK4) | `include/utils/math/nimcp_ode.h` |
| **Signal Filtering** | FIR/IIR for auditory preprocessing | `include/utils/math/nimcp_filter.h` |
| **Statistics** | Bayesian inference for word recognition | `include/utils/math/nimcp_stats.h` |
| **Linear Algebra** | SVD for dimensionality reduction | `include/utils/math/nimcp_linalg.h` |

```c
// Math utilities for phonological analysis
typedef struct {
    nimcp_fft_plan_t* fft_plan;            // Spectral analysis
    nimcp_filter_t* bandpass_filters[4];   // F1-F4 formant extraction
    nimcp_complex_t* phase_buffer;         // Neural oscillation phases
    nimcp_tensor_t* activation_tensor;     // Layer activations
} wernicke_math_ctx_t;

// Math APIs for Wernicke
int wernicke_extract_formants(wernicke_math_ctx_t* ctx,
                               const float* audio, uint32_t samples,
                               formant_t* formants);
int wernicke_compute_pac(wernicke_math_ctx_t* ctx,
                          const float* theta, const float* gamma,
                          uint32_t samples, float* coupling);
```

---

### Hypothalamus Integration

Wernicke's area integrates with hypothalamus for arousal-modulated language processing:

| Hypothalamus Module | Integration Purpose | Header |
|--------------------|---------------------|--------|
| **Drives** | Motivational context affects interpretation | `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_drives.h` |
| **Homeostasis** | Metabolic state modulates processing depth | `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_homeostasis.h` |
| **Attention Bridge** | Arousal → salience for word attention | `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_attention_bridge.h` |
| **Executive Bridge** | Drive → goal priority for comprehension focus | `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_executive_bridge.h` |
| **SNc Bridge** | Reward prediction error for learning | `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_snc_bridge.h` |

```c
// Hypothalamus-Wernicke integration
typedef struct {
    hypothalamus_attention_bridge_t* attention;   // Arousal modulation
    hypothalamus_executive_bridge_t* executive;   // Goal-directed focus
    hypothalamus_snc_bridge_t* snc;               // Dopamine learning signal
    float arousal_level;                          // Current arousal
    float processing_depth;                       // Metabolic-gated depth
} wernicke_hypothalamus_bridge_t;

// Key APIs
int wernicke_hypothalamus_modulate_attention(wernicke_hypothalamus_bridge_t* bridge,
                                              const wernicke_word_t* words,
                                              float* attention_weights);
int wernicke_hypothalamus_update_learning(wernicke_hypothalamus_bridge_t* bridge,
                                           float comprehension_reward);
```

---

### Omnidirectional Inference Integration

Wernicke's area is a core component of the omnidirectional inference system:

| Omni Module | Integration Purpose | Header |
|-------------|---------------------|--------|
| **Omni Core** | Central prediction coordination | `include/cognitive/omni/nimcp_omni_inference.h` |
| **Precision Weighting** | Confidence in phoneme/word recognition | `include/cognitive/omni/nimcp_omni_precision.h` |
| **Active Inference** | Expected free energy for interpretation | `include/cognitive/omni/nimcp_omni_active_inference.h` |
| **World Model** | Generative model for language | `include/cognitive/omni/nimcp_omni_world_model.h` |
| **Metacognition** | Self-monitoring comprehension | `include/cognitive/omni/nimcp_omni_metacognition.h` |
| **Sensory Bridge** | Cross-modal audio/visual/speech | `include/perception/nimcp_omni_sensory_bridge.h` |
| **Broca Bridge** | Language production prediction | `include/core/brain/regions/broca/nimcp_omni_broca_bridge.h` |

```c
// Omnidirectional Wernicke Bridge
typedef struct {
    omni_inference_ctx_t* omni;                // Central coordinator
    omni_precision_ctx_t* precision;           // Precision weighting
    omni_active_inference_ctx_t* active;       // EFE computation
    omni_world_model_t* world_model;           // Generative language model
    omni_metacognition_ctx_t* metacog;         // Self-monitoring

    // Wernicke-specific predictions
    float phoneme_predictions[MAX_PHONEMES];   // P(next phoneme)
    float word_predictions[MAX_VOCAB];         // P(next word)
    float semantic_predictions[MAX_CONCEPTS];  // P(meaning)
} omni_wernicke_bridge_t;

// Omnidirectional APIs
int omni_wernicke_predict_forward(omni_wernicke_bridge_t* bridge,
                                   wernicke_prediction_t* prediction);
int omni_wernicke_infer_backward(omni_wernicke_bridge_t* bridge,
                                  const semantic_concept_t* meaning,
                                  phoneme_sequence_t* expected);
int omni_wernicke_compute_precision(omni_wernicke_bridge_t* bridge,
                                     const phoneme_event_t* observed,
                                     float* precision);
int omni_wernicke_update_world_model(omni_wernicke_bridge_t* bridge,
                                      const wernicke_comprehension_t* comprehension);
```

---

### Logic Gate Integration

Wernicke's area integrates with logic systems for reasoning about language:

| Logic Module | Integration Purpose | Header |
|--------------|---------------------|--------|
| **Neural Logic Gates** | GPU-accelerated fuzzy logic for semantics | `include/cognitive/logic/nimcp_neural_logic.h` |
| **Symbolic Logic** | First-order logic for sentence meaning | `include/cognitive/logic/nimcp_symbolic_logic.h` |
| **Omni Logic Bridge** | Bidirectional inference direction | `include/cognitive/logic/nimcp_omni_logic_bridge.h` |
| **Forward Chaining** | Deductive inference from premises | `include/cognitive/logic/nimcp_forward_chaining.h` |
| **Backward Chaining** | Abductive inference for interpretation | `include/cognitive/logic/nimcp_backward_chaining.h` |
| **Analogical Reasoning** | Metaphor and analogy comprehension | `include/cognitive/logic/nimcp_analogical.h` |

```c
// Logic integration for semantic reasoning
typedef struct {
    neural_logic_ctx_t* neural;               // Fuzzy semantic logic
    symbolic_logic_ctx_t* symbolic;           // FOL representation
    omni_logic_bridge_t* omni_logic;          // Inference direction
    forward_chaining_ctx_t* forward;          // Deduction
    backward_chaining_ctx_t* backward;        // Abduction
    analogical_ctx_t* analogical;             // Metaphor processing
} wernicke_logic_ctx_t;

// Logic APIs for semantic processing
int wernicke_logic_parse_to_fol(wernicke_logic_ctx_t* ctx,
                                 const wernicke_parse_t* parse,
                                 fol_formula_t* formula);
int wernicke_logic_infer_implications(wernicke_logic_ctx_t* ctx,
                                       const fol_formula_t* premise,
                                       fol_formula_t* conclusions,
                                       uint32_t* n_conclusions);
int wernicke_logic_process_metaphor(wernicke_logic_ctx_t* ctx,
                                     const char* metaphor,
                                     semantic_mapping_t* mapping);
```

---

### All Brain Region Integrations

Wernicke's area connects to all major brain regions:

| Brain Region | Integration Purpose | Header |
|--------------|---------------------|--------|
| **Temporal Lobe** | Parent region, auditory processing | `include/core/brain/regions/temporal/nimcp_temporal_adapter.h` |
| **Broca's Area** | Language production (arcuate fasciculus) | `include/core/brain/regions/broca/nimcp_broca_adapter.h` |
| **Prefrontal Cortex** | Executive control, working memory | `include/core/brain/regions/prefrontal/nimcp_prefrontal_adapter.h` |
| **Thalamus** | MGN relay for auditory input | `include/core/brain/regions/thalamus/nimcp_thalamus_adapter.h` |
| **Basal Ganglia** | Procedural learning for syntax | `include/core/brain/regions/basal_ganglia/nimcp_basal_ganglia_adapter.h` |
| **Amygdala** | Emotional valence of words | `include/core/brain/regions/amygdala/nimcp_amygdala_adapter.h` |
| **Hippocampus** | Episodic memory for context | `include/core/brain/regions/hippocampus/nimcp_hippocampus_adapter.h` |
| **Parietal Lobe** | Spatial language, numeracy | `include/core/brain/regions/parietal/nimcp_parietal_adapter.h` |
| **Occipital Lobe** | Visual word recognition, lip reading | `include/core/brain/regions/occipital/nimcp_occipital_adapter.h` |
| **Cingulate Cortex** | Error monitoring, conflict detection | `include/core/brain/regions/cingulate/nimcp_cingulate_adapter.h` |
| **Insula** | Interoceptive context for semantics | `include/core/brain/regions/insula/nimcp_insula_adapter.h` |
| **Brainstem** | Arousal modulation | `include/core/brain/regions/brainstem/nimcp_brainstem_adapter.h` |
| **Cerebellum** | Timing prediction for speech | `include/core/brain/regions/cerebellum/nimcp_cerebellum_adapter.h` |
| **Motor Cortex** | Articulatory simulation | `include/core/brain/regions/motor/nimcp_motor_adapter.h` |
| **Hypothalamus** | Drive-based attention modulation | `include/core/brain/regions/hypothalamus/nimcp_hypothalamus_adapter.h` |

```c
// Comprehensive brain region connections
typedef struct {
    // Core language circuit
    broca_adapter_t* broca;                    // Production
    prefrontal_adapter_t* prefrontal;          // Executive

    // Sensory inputs
    temporal_adapter_t* temporal;              // Auditory
    occipital_adapter_t* occipital;            // Visual
    thalamus_adapter_t* thalamus;              // Relay

    // Memory systems
    hippocampus_adapter_t* hippocampus;        // Episodic
    basal_ganglia_adapter_t* basal_ganglia;    // Procedural

    // Emotional/motivational
    amygdala_adapter_t* amygdala;              // Valence
    hypothalamus_adapter_t* hypothalamus;      // Drives
    insula_adapter_t* insula;                  // Interoception

    // Monitoring/control
    cingulate_adapter_t* cingulate;            // Conflict
    cerebellum_adapter_t* cerebellum;          // Timing

    // Motor
    motor_adapter_t* motor;                    // Articulation

    // Arousal
    brainstem_adapter_t* brainstem;            // Arousal
} wernicke_brain_connections_t;
```

---

### Internal Knowledge Graph Integration

Wernicke's area is fully integrated with the brain knowledge graph:

| KG Module | Integration Purpose | Header |
|-----------|---------------------|--------|
| **Brain KG** | Module registration, semantic edges | `include/core/brain/nimcp_brain_kg.h` |
| **Omni KG Sync** | Self-awareness of language capabilities | `include/cognitive/omni/nimcp_omni_kg_sync.h` |
| **Semantic Memory** | Concept storage, spreading activation | `include/cognitive/memory/nimcp_semantic_memory.h` |

```c
// Knowledge Graph integration
typedef struct {
    brain_kg_t* kg;                           // Main brain KG
    omni_kg_sync_t* omni_sync;                // Omni KG sync
    semantic_memory_t* semantic;              // Concept store

    brain_kg_node_id_t wernicke_node;         // Self node in KG
    brain_kg_node_id_t broca_node;            // Broca connection
    brain_kg_node_id_t semantic_node;         // Semantic memory node
} wernicke_kg_ctx_t;

// KG registration
int wernicke_kg_register(wernicke_kg_ctx_t* ctx, brain_kg_t* kg) {
    // Register Wernicke's area as cortical node
    ctx->wernicke_node = brain_kg_add_node(kg, "Wernicke_Area",
                                            BRAIN_KG_NODE_CORTICAL,
                                            &wernicke_metadata);

    // Add edges to connected regions
    brain_kg_add_edge(kg, ctx->wernicke_node, ctx->broca_node,
                      BRAIN_KG_EDGE_CONNECTS_TO,
                      "arcuate_fasciculus", 0.95f);
    brain_kg_add_edge(kg, ctx->wernicke_node, ctx->semantic_node,
                      BRAIN_KG_EDGE_INTEGRATES_WITH,
                      "lexical_semantic", 0.9f);

    // Register with Omni KG for self-awareness
    omni_kg_capabilities_t caps = {
        .can_comprehend = true,
        .can_predict = true,
        .modalities = OMNI_MODALITY_AUDIO | OMNI_MODALITY_SPEECH
    };
    omni_kg_register_module(ctx->omni_sync, "wernicke",
                            OMNI_KG_TYPE_LANGUAGE, caps);

    return 0;
}
```

---

## Updated File Structure

```
include/core/brain/regions/wernicke/
├── nimcp_wernicke_adapter.h              # Main adapter
├── nimcp_phonological_analyzer.h         # Phoneme/syllable processing
├── nimcp_lexical_access.h                # Word recognition
├── nimcp_semantic_integrator.h           # Meaning integration
├── nimcp_syntactic_comprehension.h       # Sentence parsing
├── nimcp_wernicke_thalamic_bridge.h      # MGN relay
├── nimcp_wernicke_substrate_bridge.h     # Bio-async messaging
├── nimcp_wernicke_quantum_bridge.h       # Quantum acceleration
├── nimcp_omni_wernicke_bridge.h          # Omnidirectional inference
├── nimcp_wernicke_broca_bridge.h         # Arcuate fasciculus
├── nimcp_wernicke_gpu.h                  # GPU acceleration
├── nimcp_wernicke_hypothalamus_bridge.h  # Arousal modulation
├── nimcp_wernicke_logic.h                # Semantic reasoning
├── nimcp_wernicke_immune.h               # Immune integration
└── nimcp_wernicke_kg.h                   # Knowledge graph

src/core/brain/regions/wernicke/
├── nimcp_wernicke_adapter.c
├── nimcp_phonological_analyzer.c
├── nimcp_lexical_access.c
├── nimcp_semantic_integrator.c
├── nimcp_syntactic_comprehension.c
├── nimcp_wernicke_thalamic_bridge.c
├── nimcp_wernicke_substrate_bridge.c
├── nimcp_wernicke_quantum_bridge.c
├── nimcp_omni_wernicke_bridge.c
├── nimcp_wernicke_broca_bridge.c
├── nimcp_wernicke_gpu.c
├── nimcp_wernicke_hypothalamus_bridge.c
├── nimcp_wernicke_logic.c
├── nimcp_wernicke_immune.c
└── nimcp_wernicke_kg.c

src/gpu/cuda/wernicke/
├── nimcp_cuda_wernicke_embed.cu          # Phoneme/word embeddings
├── nimcp_cuda_wernicke_attention.cu      # Self-attention
├── nimcp_cuda_wernicke_lexicon.cu        # Lexicon lookup
└── nimcp_cuda_wernicke_formants.cu       # Formant extraction
```

---

## Updated Implementation Phases

### Phase 1: Core Adapter (Week 1-2)
- `nimcp_wernicke_adapter.h/.c` - Main lifecycle and configuration
- Basic phoneme processing pipeline
- Integration with existing speech_cortex
- Brain KG registration

### Phase 2: Processing Layers (Week 3-4)
- `nimcp_phonological_analyzer.h/.c` - Phoneme categorization
- `nimcp_lexical_access.h/.c` - Word recognition with lexicon
- `nimcp_semantic_integrator.h/.c` - Concept activation
- Math utilities integration (FFT, filters)

### Phase 3: Bridges (Week 5-6)
- `nimcp_wernicke_broca_bridge.h/.c` - Arcuate fasciculus
- `nimcp_wernicke_thalamic_bridge.h/.c` - MGN relay
- `nimcp_wernicke_substrate_bridge.h/.c` - Bio-async messaging
- `nimcp_wernicke_hypothalamus_bridge.h/.c` - Arousal modulation

### Phase 4: GPU Acceleration (Week 7-8)
- `nimcp_wernicke_gpu.h/.c` - GPU context management
- CUDA kernels for embedding, attention, lexicon
- Mixed precision support (FP16/BF16)
- Multi-GPU for large vocabularies

### Phase 5: Omnidirectional Integration (Week 9-10)
- `nimcp_omni_wernicke_bridge.h/.c` - Predictive comprehension
- KG sync for self-awareness
- Cross-modal prediction (audiovisual)
- Precision weighting integration

### Phase 6: Quantum Acceleration (Week 11-12)
- `nimcp_wernicke_quantum_bridge.h/.c` - Quantum context
- QMC for semantic sampling
- QMCTS for parse tree search
- Quantum walks for spreading activation

### Phase 7: Logic & Reasoning (Week 13-14)
- `nimcp_wernicke_logic.h/.c` - Logic integration
- FOL representation of sentence meaning
- Forward/backward chaining for inference
- Analogical reasoning for metaphors

### Phase 8: Advanced Features (Week 15-16)
- Syntactic comprehension
- Context-dependent disambiguation
- Self-monitoring via Broca feedback
- All brain region connections

---

## References

1. Wernicke, C. (1874). Der aphasische Symptomencomplex
2. Hickok, G., & Poeppel, D. (2007). The cortical organization of speech processing
3. Friederici, A. D. (2012). The cortical language circuit
4. Hagoort, P. (2014). Nodes and networks in the neural architecture for language
5. Friston, K. (2010). The free-energy principle (for predictive comprehension)
6. NIMCP GPU Architecture Documentation (internal)
7. NIMCP Quantum Algorithms Documentation (internal)
8. NIMCP Omnidirectional Inference Documentation (internal)
