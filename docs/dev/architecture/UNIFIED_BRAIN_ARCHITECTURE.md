# NIMCP Unified Brain Architecture

**Problem Statement**: Current modules are isolated silos. Sensory cortices don't feed into the neural network. Cognitive modules operate independently. No shared substrate or integration layer.

**Solution**: Unified processing pipeline where all modules share neural substrate and coordinate during inference.

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                         NIMCP UNIFIED BRAIN                       │
├──────────────────────────────────────────────────────────────────┤
│                                                                    │
│  INPUT STAGE: Multi-Modal Sensory Processing                     │
│  ════════════════════════════════════════════                    │
│                                                                    │
│  [Image]  → Visual Cortex (CNN)  → visual_features[128]          │
│  [Audio]  → Audio Cortex (FFT)   → audio_features[64]            │
│  [Vector] → Direct Input         → direct_features[N]            │
│                                                                    │
│                              ↓                                     │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │         MULTI-MODAL INTEGRATION LAYER                      │  │
│  │  • Feature concatenation: [visual|audio|direct]            │  │
│  │  • Attention-based weighting                               │  │
│  │  • Dimensionality reduction                                │  │
│  │  • Unified representation → integrated_features[256]       │  │
│  └────────────────────────┬───────────────────────────────────┘  │
│                           ↓                                        │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │              CORE NEURAL NETWORK                           │  │
│  │  ═══════════════════════════════                           │  │
│  │                                                            │  │
│  │  INPUT LAYER:                                              │  │
│  │  • Receives integrated_features                            │  │
│  │  • Maps to input neurons [0..num_inputs-1]                │  │
│  │                                                            │  │
│  │  HIDDEN LAYERS (Recurrent Spiking Network):               │  │
│  │  • Neurons with STDP learning                              │  │
│  │  • Synaptic connections                                    │  │
│  │  • Shared by ALL modules                                   │  │
│  │                                                            │  │
│  │  SPECIALIZED REGIONS (Same Network):                       │  │
│  │  ┌─────────────────────────────────────────────┐          │  │
│  │  │ Visual Processing Region (neurons 0-1000)    │          │  │
│  │  │ - Receives visual features                   │          │  │
│  │  │ - Learns visual patterns via STDP            │          │  │
│  │  └─────────────────────────────────────────────┘          │  │
│  │  ┌─────────────────────────────────────────────┐          │  │
│  │  │ Audio Processing Region (neurons 1001-2000)  │          │  │
│  │  │ - Receives audio features                    │          │  │
│  │  │ - Learns audio patterns via STDP             │          │  │
│  │  └─────────────────────────────────────────────┘          │  │
│  │  ┌─────────────────────────────────────────────┐          │  │
│  │  │ Integration Region (neurons 2001-3000)       │          │  │
│  │  │ - Combines visual + audio                    │          │  │
│  │  │ - Multi-modal association learning           │          │  │
│  │  └─────────────────────────────────────────────┘          │  │
│  │  ┌─────────────────────────────────────────────┐          │  │
│  │  │ Cognitive Region (neurons 3001-4000)         │          │  │
│  │  │ - Higher-order reasoning                     │          │  │
│  │  │ - Decision making                            │          │  │
│  │  └─────────────────────────────────────────────┘          │  │
│  │                                                            │  │
│  │  OUTPUT LAYER:                                             │  │
│  │  • Maps to output neurons                                  │  │
│  │  • Pre-integrated representation                           │  │
│  │                                                            │  │
│  │  BIOLOGICAL MECHANISMS (Apply to ALL regions):            │  │
│  │  ✓ STDP - Spike-timing-dependent plasticity               │  │
│  │  ✓ Glial Cells - Astrocytes, oligodendrocytes, microglia  │  │
│  │  ✓ Brain Oscillations - Delta, theta, alpha, beta, gamma  │  │
│  │  ✓ Pink Noise Neuromodulation - Dopamine, serotonin, etc. │  │
│  │  ✓ Eligibility Traces - Temporal credit assignment        │  │
│  └────────────────────────┬───────────────────────────────────┘  │
│                           ↓                                        │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │         COGNITIVE PROCESSING LAYER                         │  │
│  │  ═══════════════════════════════                           │  │
│  │                                                            │  │
│  │  Operates on neural network state:                         │  │
│  │                                                            │  │
│  │  • Introspection: Analyzes network uncertainty             │  │
│  │  • Salience: Identifies most relevant neurons/patterns     │  │
│  │  • Ethics: Evaluates decision against Golden Rule          │  │
│  │  • Curiosity: Detects novel patterns in network activity   │  │
│  │  • Knowledge: Integrates multi-domain information          │  │
│  └────────────────────────┬───────────────────────────────────┘  │
│                           ↓                                        │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │         PRE-OUTPUT INTEGRATION                             │  │
│  │  ═══════════════════════                                   │  │
│  │                                                            │  │
│  │  • Consolidation: Strengthen important patterns            │  │
│  │  • Knowledge Integration: Apply learned constraints        │  │
│  │  • Ethical Filtering: Block harmful outputs                │  │
│  │  • Salience Weighting: Focus on relevant outputs           │  │
│  │  • Final Readout: Extract output from network state        │  │
│  └────────────────────────┬───────────────────────────────────┘  │
│                           ↓                                        │
│                    [OUTPUT VECTOR]                                │
│                                                                    │
└──────────────────────────────────────────────────────────────────┘
```

---

## Key Design Principles

### 1. **Single Shared Neural Network**
- ONE neural_network_t with all neurons/synapses
- Visual, audio, cognitive functions all use SAME neurons
- Different regions assigned different roles (like real brain)
- All share glial cells, STDP, oscillations

### 2. **Feature-Based Sensory Input**
- Visual/audio cortices = specialized feature extractors (CNN/FFT)
- Features feed INTO input neurons of main network
- Network learns to integrate multi-modal features
- Biologically accurate: V1 is still cortical tissue

### 3. **Region-Based Specialization**
- Neuron ID ranges define functional regions:
  - Visual region: [0, num_visual_neurons)
  - Audio region: [num_visual, num_visual+num_audio)
  - Integration: [num_visual+num_audio, num_integration)
  - Cognitive: [num_integration, total_neurons)
- Connectivity biased WITHIN regions but allows BETWEEN

### 4. **Cognitive Modules as Observers**
- Cognitive modules READ network state
- They don't have separate neurons
- Ethics checks network output
- Introspection analyzes network confidence
- Salience identifies important patterns

### 5. **Unified Processing Pipeline**
```c
brain_decision_t brain_process_unified(
    brain_t brain,
    const uint8_t* visual_input,    // Optional
    const float* audio_input,       // Optional
    const float* direct_input,      // Optional
    brain_decision_t* decision
)
{
    // 1. SENSORY STAGE
    float visual_features[128] = {0};
    float audio_features[64] = {0};

    if (visual_input && brain->visual_cortex)
        visual_cortex_process(brain->visual_cortex, visual_input, ...);

    if (audio_input && brain->audio_cortex)
        audio_cortex_process(brain->audio_cortex, audio_input, ...);

    // 2. INTEGRATION STAGE
    float integrated[256];
    integrate_modalities(visual_features, audio_features, direct_input, integrated);

    // 3. NEURAL PROCESSING
    neural_network_step(brain->network, integrated);

    // Apply biological mechanisms (STDP, glial, oscillations)
    stdp_update(brain->network);
    glial_integration_step(brain->glial);
    brain_oscillations_analyze(brain->oscillations);

    // 4. COGNITIVE PROCESSING
    float confidence = introspection_assess_confidence(brain->introspection, brain->network);
    float salience = salience_compute(brain->salience, brain->network);
    bool ethical = ethics_check_output(brain->ethics, output_neurons);

    // 5. PRE-OUTPUT INTEGRATION
    if (!ethical) {
        // Block unethical outputs
        modify_output_to_safe_default();
    }

    // Weight output by salience
    apply_salience_weighting(output_neurons, salience);

    // 6. EXTRACT OUTPUT
    extract_output(brain->network, decision);

    return decision;
}
```

---

## Implementation Plan

### Phase 1: Multi-Modal Integration Layer ✅
**File**: `src/core/integration/nimcp_multimodal_integration.h/c`

```c
typedef struct {
    float* visual_features;
    uint32_t visual_dim;
    float* audio_features;
    uint32_t audio_dim;
    float* direct_features;
    uint32_t direct_dim;
} multimodal_input_t;

typedef struct {
    float* integrated_features;
    uint32_t integrated_dim;
    float* attention_weights;  // Per-modality attention
} multimodal_output_t;

multimodal_output_t* multimodal_integrate(
    const multimodal_input_t* input,
    integration_config_t* config
);
```

**Features**:
- Concatenation (simple)
- Attention-based fusion (learned weights)
- Cross-modal correlation
- Dimensionality reduction

### Phase 2: Neural Network Region Assignment
**File**: `src/core/brain/nimcp_brain_regions.h/c`

```c
typedef struct {
    uint32_t start_neuron_id;
    uint32_t end_neuron_id;
    char name[64];
    brain_region_type_t type;  // SENSORY, INTEGRATION, COGNITIVE, OUTPUT
} brain_region_t;

typedef struct {
    brain_region_t* regions;
    uint32_t num_regions;
    neural_network_t network;  // Shared network
} brain_architecture_t;

// Define regions during brain creation
brain_region_t* brain_add_region(brain_t brain, const char* name,
                                 uint32_t num_neurons, brain_region_type_t type);

// Get neurons in region
neuron_t** brain_get_region_neurons(brain_t brain, brain_region_t* region);
```

### Phase 3: Unified Processing Function
**File**: `src/core/brain/nimcp_brain.c` (extend existing)

```c
brain_decision_t brain_process_multimodal(
    brain_t brain,
    const uint8_t* visual_input,   // Can be NULL
    uint32_t visual_width,
    uint32_t visual_height,
    const float* audio_input,      // Can be NULL
    uint32_t audio_samples,
    const float* direct_input,     // Can be NULL
    uint32_t direct_dim
);
```

### Phase 4: Cognitive Module Integration
**Each cognitive module gets network access**:

```c
// Ethics sees network output before returning decision
bool ethics_validate_network_output(
    ethics_engine_t ethics,
    neural_network_t network,
    uint32_t* output_neuron_ids,
    uint32_t num_outputs
);

// Introspection analyzes network state
float introspection_compute_network_confidence(
    introspection_context_t introspection,
    neural_network_t network
);

// Salience identifies important neurons
uint32_t* salience_get_important_neurons(
    salience_evaluator_t salience,
    neural_network_t network,
    uint32_t* num_neurons
);
```

### Phase 5: Pre-Output Integration
**File**: `src/core/brain/nimcp_brain_output.h/c`

```c
typedef struct {
    float* raw_output;
    float confidence;
    float salience;
    bool ethical_approved;
    char explanation[256];
} integrated_output_t;

integrated_output_t* brain_integrate_output(
    brain_t brain,
    neural_network_t network
);
```

---

## Benefits

1. **Single Source of Truth**: One neural network, all modules share it
2. **Biological Accuracy**: Glial, STDP, oscillations apply universally
3. **Multi-Modal**: Vision + audio + other inputs naturally integrated
4. **Cognitive Awareness**: Ethics/introspection see full network state
5. **Coordinated**: All modules work together, not in isolation
6. **Efficient**: No duplicate neural structures

---

## Next Steps

1. ✅ Create this design document
2. Implement `nimcp_multimodal_integration` module
3. Add brain region assignments to brain_t structure
4. Implement `brain_process_multimodal()` function
5. Wire sensory cortices to network inputs
6. Add cognitive module hooks to processing pipeline
7. Implement pre-output integration
8. Test end-to-end with visual + audio + ethics

---

**Author**: NIMCP Development Team
**Date**: 2025-11-08
**Version**: 2.7.0 Phase 8 - Unified Architecture
