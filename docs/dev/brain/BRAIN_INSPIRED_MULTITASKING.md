# Brain-Inspired Hierarchical Multi-Tasking Architecture

## Overview

This document describes NIMCP's brain-inspired approach to multi-tasking, modeled after the hierarchical organization of the human brain. Unlike traditional neural networks, this architecture uses specialized "brain regions" that communicate and coordinate, just like the real brain.

## Biological Inspiration

### Human Brain Organization

```
Cerebral Cortex
├── Sensory Cortex (Input Processing)
│   ├── Visual Cortex (V1, V2, V4, IT)
│   ├── Auditory Cortex (A1, A2)
│   └── Somatosensory Cortex (S1, S2)
│
├── Association Cortex (Integration)
│   ├── Parietal (Spatial reasoning)
│   ├── Temporal (Memory, language)
│   └── Prefrontal (Executive control)
│
├── Motor Cortex (Output/Action)
│   ├── Primary Motor (M1)
│   └── Premotor (Planning)
│
└── Subcortical Structures
    ├── Basal Ganglia (Action selection)
    ├── Hippocampus (Memory)
    ├── Amygdala (Emotion)
    └── Thalamus (Relay station)
```

### Key Principles

1. **Hierarchical Processing**: Information flows from simple to complex
2. **Specialized Regions**: Each area has specific functions
3. **Lateral Connections**: Regions communicate with peers
4. **Feedback Loops**: Higher areas influence lower areas
5. **Parallel Pathways**: Multiple processing streams
6. **Modularity**: Regions can be added/removed
7. **Plasticity**: Connections adapt with experience

## NIMCP Hierarchical Architecture

### Architecture Diagram

```
Input Layer (Sensory Cortex)
    ↓
┌─────────────────────────────────────────────┐
│  PRIMARY PROCESSING (V1-like)               │
│  - Edge detection                           │
│  - Basic feature extraction                 │
│  - Low-level patterns                       │
└─────────────────────────────────────────────┘
    ↓           ↓           ↓
┌──────────┐ ┌──────────┐ ┌──────────┐
│ Region 1 │ │ Region 2 │ │ Region 3 │  ← SECONDARY (V2/V4-like)
│ Shape    │ │ Color    │ │ Texture  │    Specialized processing
└──────────┘ └──────────┘ └──────────┘
    ↓           ↓           ↓
    └───────────┴───────────┘
             ↓
┌─────────────────────────────────────────────┐
│  ASSOCIATION CORTEX (IT-like)               │
│  - Object recognition                       │
│  - Semantic understanding                   │
│  - Context integration                      │
└─────────────────────────────────────────────┘
             ↓
┌─────────────────────────────────────────────┐
│  PREFRONTAL CORTEX (Executive Control)      │
│  - Task selection                           │
│  - Decision making                          │
│  - Goal management                          │
└─────────────────────────────────────────────┘
             ↓
    ┌────────┴────────┐
    ↓        ↓        ↓
┌────────┐ ┌────────┐ ┌────────┐
│ Task 1 │ │ Task 2 │ │ Task 3 │  ← OUTPUT REGIONS
│ What?  │ │ Where? │ │ How?   │    Task-specific
└────────┘ └────────┘ └────────┘
```

## Brain Region Types

### 1. Sensory Regions (Input Processing)

**Biological Model**: Primary Visual Cortex (V1), Primary Auditory Cortex (A1)

**Function**:
- Receive raw input
- Extract basic features
- Topographic organization
- Rapid, parallel processing

**NIMCP Implementation**:
```c
nimcp_brain_t sensory_cortex = nimcp_brain_create(
    "visual_v1",
    NIMCP_BRAIN_SMALL,  // Fast processing
    NIMCP_TASK_PATTERN_MATCHING,
    784,   // Raw input (e.g., 28x28 image)
    128    // Feature representation
);
```

### 2. Association Regions (Integration)

**Biological Model**: Inferotemporal Cortex (IT), Parietal Cortex

**Function**:
- Combine features from multiple sensory regions
- Build complex representations
- Context-dependent processing
- Semantic understanding

**NIMCP Implementation**:
```c
nimcp_brain_t association_cortex = nimcp_brain_create(
    "temporal_it",
    NIMCP_BRAIN_MEDIUM,  // More complex
    NIMCP_TASK_ASSOCIATION,
    384,   // Combined features (128 × 3 regions)
    256    // Integrated representation
);
```

### 3. Executive Regions (Control)

**Biological Model**: Prefrontal Cortex (PFC), Dorsolateral PFC

**Function**:
- Task switching
- Goal management
- Working memory
- Decision making
- Attention control

**NIMCP Implementation**:
```c
nimcp_brain_t prefrontal_cortex = nimcp_brain_create(
    "pfc_executive",
    NIMCP_BRAIN_SMALL,
    NIMCP_TASK_SEQUENCE,  // Sequential decision making
    256,   // Integrated representation
    64     // Control signals
);
```

### 4. Motor/Output Regions

**Biological Model**: Primary Motor Cortex (M1), Premotor Cortex

**Function**:
- Generate task-specific outputs
- Action selection
- Fine motor control
- Response execution

**NIMCP Implementation**:
```c
nimcp_brain_t motor_cortex_task1 = nimcp_brain_create(
    "motor_classification",
    NIMCP_BRAIN_TINY,
    NIMCP_TASK_CLASSIFICATION,
    64,    // Control signals
    10     // Task output
);
```

### 5. Subcortical Structures

**Basal Ganglia** - Action Selection:
```c
// Competitive selection among actions
// Winner-take-all dynamics
nimcp_brain_t basal_ganglia = nimcp_brain_create(
    "action_selector",
    NIMCP_BRAIN_TINY,
    NIMCP_TASK_CLASSIFICATION,
    num_actions,
    1  // Selected action
);
```

**Hippocampus** - Memory:
```c
// Episodic memory storage
// Pattern completion
nimcp_brain_t hippocampus = nimcp_brain_create(
    "memory_system",
    NIMCP_BRAIN_MEDIUM,
    NIMCP_TASK_ASSOCIATION,
    context_size,
    memory_size
);
```

## Information Flow Patterns

### 1. Feedforward (Bottom-Up)

```
Sensory Input → V1 → V2 → V4 → IT → PFC → Output
```

**Characteristics**:
- Fast processing
- Stimulus-driven
- Automatic responses
- Pattern recognition

### 2. Feedback (Top-Down)

```
PFC → IT → V4 → V2 → V1 → Sensory
```

**Characteristics**:
- Attention modulation
- Expectation-based
- Context influence
- Predictive coding

### 3. Lateral (Horizontal)

```
V1-shape ↔ V1-color ↔ V1-texture
```

**Characteristics**:
- Feature binding
- Contrast enhancement
- Contextual modulation
- Synchronization

## Communication Mechanisms

### 1. Synaptic Connections (Direct)

```c
// Direct connection between regions
float* v1_output = get_region_output(visual_v1);
nimcp_brain_learn_example(visual_v2, v1_output, 128, label, 1.0);
```

### 2. Neuromodulation (Indirect)

```c
// Dopamine-like signal modulates learning
float reward = compute_reward(prediction, actual);
adjust_learning_rate(all_regions, reward);  // Broadcast signal
```

### 3. Oscillatory Coupling

```c
// Gamma oscillations (40 Hz) for binding
// Theta oscillations (4-8 Hz) for memory
// Simulated via temporal correlation
```

### 4. Attention Gating

```c
// Prefrontal cortex gates information flow
float attention_weights[num_regions];
compute_attention(pfc_state, attention_weights);

// Apply attention to each region
for (int i = 0; i < num_regions; i++) {
    scale_activity(regions[i], attention_weights[i]);
}
```

## Plasticity Mechanisms

### 1. Hebbian Learning (Local)

**"Neurons that fire together, wire together"**

```c
// Within-region plasticity
plasticity:
  enable_bcm: true     # Bienenstock-Cooper-Munro
  bcm_tau: 1000.0
```

### 2. Spike-Timing Dependent Plasticity (STDP)

**Timing matters for causality**

```c
// Between-region plasticity
plasticity:
  enable_stdp: true    # Temporal correlations
  stdp_window: 20.0    # 20ms window
```

### 3. Neuromodulation (Global)

**Dopamine, Serotonin, Acetylcholine**

```c
// Global learning signals
float dopamine = reward_prediction_error();
float serotonin = confidence_level();
float acetylcholine = surprise_signal();

// Modulate all regions
broadcast_neuromodulator(all_regions, dopamine, serotonin, acetylcholine);
```

### 4. Homeostatic Plasticity

**Maintain stability**

```c
// Prevent runaway excitation/inhibition
for each region:
    adjust_excitability_to_target()
    normalize_synaptic_weights()
```

## Task Decomposition Strategies

### Strategy 1: Dorsal/Ventral Streams

**"What" vs "Where"**

```
Visual Input
    ↓
   V1
    ↓
  ┌─┴─┐
  ↓   ↓
 V2  V2
  ↓   ↓
┌──┐ ┌──┐
│V4│ │V5│
└──┘ └──┘
 ↓    ↓
IT   Parietal
 ↓    ↓
What  Where
```

### Strategy 2: Multiple Specialized Pathways

**Parallel processing streams**

```
Input
  ↓
┌─┴─┬─┬─┐
↓   ↓ ↓ ↓
Color Shape Motion Depth
↓   ↓ ↓ ↓
└─┬─┴─┴─┘
  ↓
Binding
  ↓
Output
```

### Strategy 3: Hierarchical Refinement

**Coarse to fine processing**

```
Level 1: Is it an object?
Level 2: Is it an animal?
Level 3: Is it a mammal?
Level 4: Is it a cat?
Level 5: Which cat breed?
```

## Cognitive Functions

### Working Memory

**Maintain information temporarily**

```c
typedef struct {
    nimcp_brain_t dlpfc;        // Dorsolateral PFC
    float* buffer;              // Active representations
    int buffer_size;
    float decay_rate;           // Forgetting
} working_memory_t;

void working_memory_update(working_memory_t* wm, float* new_info) {
    // Decay old information
    for (int i = 0; i < wm->buffer_size; i++) {
        wm->buffer[i] *= wm->decay_rate;
    }

    // Add new information
    // Maintain via persistent activity in DLPFC
    nimcp_brain_predict(wm->dlpfc, new_info, ...);
}
```

### Attention

**Select relevant information**

```c
typedef struct {
    nimcp_brain_t fef;          // Frontal Eye Fields
    nimcp_brain_t ppc;          // Posterior Parietal Cortex
    float* salience_map;
    float* goal_signals;
} attention_system_t;

void compute_attention(attention_system_t* attn,
                       float* bottom_up,
                       float* top_down,
                       float* attention_weights) {
    // Bottom-up salience (stimulus-driven)
    compute_salience(bottom_up, attn->salience_map);

    // Top-down goals (task-driven)
    nimcp_brain_predict(attn->fef, top_down, ...);

    // Combine
    for (int i = 0; i < N; i++) {
        attention_weights[i] = attn->salience_map[i] * goal_signals[i];
    }
}
```

### Executive Control

**Manage multiple tasks**

```c
typedef struct {
    nimcp_brain_t dlpfc;        // Planning
    nimcp_brain_t ofc;          // Value assessment
    nimcp_brain_t acc;          // Conflict monitoring
    float* task_goals;
    float* task_priorities;
} executive_system_t;

int select_task(executive_system_t* exec, float* context) {
    // Monitor for conflicts
    float conflict = detect_conflict(exec->acc, context);

    // Adjust control if needed
    if (conflict > threshold) {
        increase_cognitive_control(exec->dlpfc);
    }

    // Select highest priority task
    return argmax(exec->task_priorities);
}
```

## Advantages of Brain-Inspired Architecture

### 1. Modularity
- Add/remove regions without retraining everything
- Isolate and debug specific functions
- Incremental development

### 2. Interpretability
- Each region has clear function
- Information flow is explicit
- Easier to understand decisions

### 3. Biological Plausibility
- Matches known brain organization
- Can leverage neuroscience insights
- Potential for brain-computer interfaces

### 4. Robustness
- Damage to one region doesn't destroy all functionality
- Graceful degradation
- Redundant pathways

### 5. Transfer Learning
- Low-level regions reusable across tasks
- Abstract representations emerge naturally
- Few-shot learning possible

### 6. Multi-Scale Learning
- Different learning rates for different regions
- Slow high-level concepts, fast low-level details
- Matches biological timescales

## Implementation Roadmap

### Phase 1: Core Regions (CURRENT)
- ✅ Create individual brain regions
- ✅ Define information flow
- ✅ Implement basic communication

### Phase 2: Plasticity (IN PROGRESS)
- ✅ BCM for within-region learning
- ✅ STDP for between-region learning
- ⏳ Neuromodulation system

### Phase 3: Executive Functions
- ⏳ Working memory implementation
- ⏳ Attention mechanisms
- ⏳ Task switching

### Phase 4: Advanced Features
- ⏳ Predictive coding
- ⏳ Oscillatory dynamics
- ⏳ Consciousness-like integration

## Next Steps

See the following files for implementation:
- `configs/templates/hierarchical_brain_regions.yaml` - Configuration
- `examples/hierarchical_multitask.c` - Full example
- `src/cognitive/cortex/` - Region implementations (future)

## References

1. Felleman & Van Essen (1991) - "Distributed hierarchical processing in the primate cerebral cortex"
2. Hassabis et al. (2017) - "Neuroscience-Inspired Artificial Intelligence"
3. Kell & McDermott (2019) - "Deep neural network models of sensory systems"
4. Yamins & DiCarlo (2016) - "Using goal-driven deep learning models to understand sensory cortex"
