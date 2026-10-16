# Prime Resonant Cognitive Architecture - Implementation Plan

**Version**: 1.0.0
**Created**: 2026-01-08
**Status**: Approved for Implementation

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [System Overview](#2-system-overview)
3. [Core Architecture](#3-core-architecture)
4. [Implementation Phases](#4-implementation-phases)
5. [Enhancement Specifications](#5-enhancement-specifications)
6. [Module Integration Updates](#6-module-integration-updates)
7. [File Inventory](#7-file-inventory)
8. [Dependencies and Reuse](#8-dependencies-and-reuse)
9. [Testing Strategy](#9-testing-strategy)
10. [Risk Assessment](#10-risk-assessment)

---

## 1. Executive Summary

### 1.1 Purpose

The Prime Resonant Cognitive Architecture provides NIMCP with a unified, biologically-realistic memory system that enables:

- **Content-addressable retrieval** via prime signatures
- **Semantic state encoding** via quaternions (consolidation, emotion, salience, accessibility)
- **Automatic association** via entanglement graphs
- **Multi-timescale consolidation** via Z-ladder tiers
- **Neural-realistic dynamics** via theta-gamma coupling and pink noise
- **Advanced memory capabilities**: metamemory, prospective memory, procedural memory, schemas, and more

### 1.2 Value Proposition

| Capability | Before | After |
|------------|--------|-------|
| Memory Query | Index-based lookup | Content-based + spreading activation |
| Associations | Manual linking | Automatic resonance-based entanglement |
| Forgetting | Manual deletion | Natural decay by tier + importance |
| Emotional Memory | Not supported | Quaternion x-component |
| Skills/Habits | Not distinguished | Separate procedural system |
| Future Planning | Not supported | Episodic future simulation |
| Self-Awareness | None | Full metamemory system |
| Social Memory | None | Person nodes + collective sync |

### 1.3 Scope

- **New Headers**: 53
- **New Sources**: 53
- **Estimated Lines**: ~88,000
- **Modules Updated**: All major NIMCP modules (25+)

---

## 2. System Overview

### 2.1 Conceptual Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                      PRIME RESONANT COGNITIVE ARCHITECTURE                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                        MEMORY INTERFACE LAYER                        │    │
│  │  pr_encode() | pr_retrieve() | pr_associate() | pr_consolidate()    │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                                      │                                       │
│  ┌───────────────────────────────────┼───────────────────────────────────┐  │
│  │                                   │                                   │  │
│  │  ┌─────────────┐  ┌─────────────┐ │ ┌─────────────┐  ┌─────────────┐ │  │
│  │  │   PRIME     │  │ QUATERNION  │ │ │ ENTANGLE-   │  │  Z-LADDER   │ │  │
│  │  │ SIGNATURES  │  │   STATE     │ │ │ MENT GRAPH  │  │   TIERS     │ │  │
│  │  │             │  │             │ │ │             │  │             │ │  │
│  │  │ Content →   │  │ w=consolidate│ │ │ Automatic   │  │ Z0: Working │ │  │
│  │  │ Unique ID   │  │ x=emotion   │ │ │ association │  │ Z1: Short   │ │  │
│  │  │             │  │ y=salience  │ │ │ via         │  │ Z2: Long    │ │  │
│  │  │ Similarity  │  │ z=access    │ │ │ resonance   │  │ Z3: Perm    │ │  │
│  │  └─────────────┘  └─────────────┘ │ └─────────────┘  └─────────────┘ │  │
│  │                    CORE LAYER                                         │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                      │                                       │
│  ┌───────────────────────────────────┼───────────────────────────────────┐  │
│  │                                   │                                   │  │
│  │  ┌─────────────┐  ┌─────────────┐ │ ┌─────────────┐  ┌─────────────┐ │  │
│  │  │  THETA-     │  │   PINK      │ │ │  KURAMOTO   │  │  RESONANCE  │ │  │
│  │  │  GAMMA      │  │   NOISE     │ │ │ OSCILLATORS │  │   SCORING   │ │  │
│  │  │             │  │             │ │ │             │  │             │ │  │
│  │  │ Encode/     │  │ 1/f         │ │ │ Module      │  │ Jaccard +   │ │  │
│  │  │ Retrieve    │  │ modulation  │ │ │ sync        │  │ Phase +     │ │  │
│  │  │ windows     │  │ Fractal     │ │ │             │  │ Quat +      │ │  │
│  │  │             │  │ timing      │ │ │             │  │ Kuramoto    │ │  │
│  │  └─────────────┘  └─────────────┘ │ └─────────────┘  └─────────────┘ │  │
│  │                    DYNAMICS LAYER                                     │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                      │                                       │
│  ┌───────────────────────────────────┼───────────────────────────────────┐  │
│  │                         ENHANCEMENT LAYER                             │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐         │  │
│  │  │ META-   │ │PROSPEC- │ │RECON-   │ │PROCE-   │ │ FUTURE  │         │  │
│  │  │ MEMORY  │ │TIVE     │ │SOLIDATE │ │DURAL    │ │THINKING │         │  │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘         │  │
│  │  ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐         │  │
│  │  │ SOURCE  │ │ SOCIAL  │ │ SCHEMAS │ │ SPACED  │ │FLASHBULB│         │  │
│  │  │ MEMORY  │ │ MEMORY  │ │ & GIST  │ │REPETITION│ │ MEMORY  │         │  │
│  │  └─────────┘ └─────────┘ └─────────┘ └─────────┘ └─────────┘         │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                      │                                       │
│  ┌───────────────────────────────────┼───────────────────────────────────┐  │
│  │                         INTEGRATION LAYER                             │  │
│  │                                                                       │  │
│  │  SNN │ Plasticity │ Training │ Perception │ Bio-Async │ Immune │ ... │  │
│  │                                                                       │  │
│  └───────────────────────────────────────────────────────────────────────┘  │
│                                                                              │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow

```
ENCODING PATH:
  Perception → Prime Signature → Quaternion State → Z0 → Z1 → Z2 → Z3
                     ↓                   ↓
              Entanglement         Flashbulb Check
                 Graph              (emotional?)

RETRIEVAL PATH:
  Query → Prime Sig Match → Resonance Score → Pink Noise Mod → Top-K
                ↓                  ↓                              ↓
          Quantum Walk      Theta Phase Gate            Spreading Activation

CONSOLIDATION PATH:
  Fractal Timer → Eligibility Check → STDP Update → Z-Ladder Promotion
        ↓                ↓                  ↓
   Pink Noise       Dopamine Gate     Entanglement Strengthen
```

---

## 3. Core Architecture

### 3.1 Prime Signature System

**Purpose**: Content-addressable memory indexing

**Structure**:
```c
typedef struct {
    uint64_t primes[PRIME_SIG_DIM];   // First 64 primes
    uint8_t exponents[PRIME_SIG_DIM]; // Exponents (0-255)
    uint64_t hash;                    // Quick comparison
} prime_signature_t;
```

**Operations**:
- `prime_sig_from_content(data, size)` → signature
- `prime_sig_similarity(s1, s2)` → Jaccard coefficient
- `prime_sig_compose(s1, s2)` → union of prime factors
- `prime_sig_hash(sig)` → 64-bit hash

### 3.2 Quaternion State

**Purpose**: Semantic metadata for each memory

**Components**:
| Component | Range | Meaning |
|-----------|-------|---------|
| w | 0-1 | Consolidation strength |
| x | -1 to +1 | Emotional valence |
| y | 0-1 | Salience/attention weight |
| z | 0-1 | Accessibility/retrieval ease |

**Operations**:
- `quat_hamilton_product(q1, q2)` → non-commutative multiply
- `quat_slerp(q1, q2, t)` → spherical interpolation
- `quat_geodesic_distance(q1, q2)` → angular distance

### 3.3 Entanglement Graph

**Purpose**: Automatic association between memories

**Edge Structure**:
```c
typedef struct {
    uint64_t from_id;
    uint64_t to_id;
    float resonance_score;    // Combined R metric
    float prime_similarity;   // Jaccard on primes
    float quat_similarity;    // Geodesic distance
    float phase_coherence;    // PLV between nodes
    entangle_edge_type_t type; // SEMANTIC, TEMPORAL, CAUSAL
} entangle_edge_t;
```

### 3.4 Z-Ladder Memory Tiers

| Tier | Decay τ | Capacity | Purpose | COW Strategy |
|------|---------|----------|---------|--------------|
| Z0 | Seconds | 7±2 | Working memory | Direct alloc |
| Z1 | Hours | ~100 | Short-term | Direct alloc |
| Z2 | Days | ~10,000 | Long-term | Object COW |
| Z3 | None | Unlimited | Permanent | Page COW |

### 3.5 Resonance Scoring

**Formula**:
```
R = w₁·Jaccard + w₂·Phase + w₃·Quat + w₄·Kuramoto

Where:
  Jaccard = prime_sig_similarity(query, memory)
  Phase = phasor_phase_coherence(query_phase, memory_phase)
  Quat = 1 - quat_geodesic_distance(query_quat, memory_quat)
  Kuramoto = oscillator_coherence(query_module, memory_module)
```

---

## 4. Implementation Phases

### Phase 1: Core Math Foundations

**Duration**: Foundation layer, implement first

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 1.1 | Quaternion Math | nimcp_quaternion.h | nimcp_quaternion.c | 2,000 |
| 1.2 | Pink Noise Extension | nimcp_pr_pink_noise.h | nimcp_pr_pink_noise.c | 1,500 |
| 1.3 | Fractal Analysis | nimcp_fractal.h | nimcp_fractal.c | 1,400 |

**Dependencies**: nimcp_complex_math.h, nimcp_fft.h, nimcp_pink_noise.h (existing)

**Deliverables**:
- Hamilton product, SLERP, quaternion exponential/logarithm
- Voss-McCartney generator, spectral shaping, quaternionic pink noise
- Hurst exponent, DFA, spectral slope estimation

---

### Phase 2: Prime Resonant Core

**Duration**: Core memory primitives

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 2.1 | Prime Signature | nimcp_prime_signature.h | nimcp_prime_signature.c | 1,100 |
| 2.2 | Resonance Engine | nimcp_resonance.h | nimcp_resonance.c | 1,600 |
| 2.3 | Kuramoto Oscillators | nimcp_kuramoto.h | nimcp_kuramoto.c | 2,000 |

**Dependencies**: Phase 1, nimcp_integration.h (RK4)

**Deliverables**:
- Content → prime signature encoding
- Four-component resonance scoring
- Module synchronization via coupled oscillators

---

### Phase 3: Memory Graph Infrastructure

**Duration**: Storage layer

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 3.1 | PR Memory Node | nimcp_pr_memory_node.h | nimcp_pr_memory_node.c | 1,400 |
| 3.2 | Entanglement Graph | nimcp_entanglement.h | nimcp_entanglement.c | 2,500 |
| 3.3 | KG Bridge | nimcp_pr_kg_bridge.h | nimcp_pr_kg_bridge.c | 1,100 |

**Dependencies**: Phase 2, nimcp_cow_manager.h, nimcp_unified_memory.h, nimcp_brain_kg.h

**Deliverables**:
- COW-enabled memory nodes
- Sparse entanglement graph with quantum walk search
- Integration with existing knowledge graph

---

### Phase 4: Spectral Memory Layer

**Duration**: Timing and multi-scale

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 4.1 | Theta-Gamma Coupling | nimcp_theta_gamma.h | nimcp_theta_gamma.c | 1,600 |
| 4.2 | Z-Ladder Memory | nimcp_z_ladder.h | nimcp_z_ladder.c | 1,900 |

**Dependencies**: Phase 3, nimcp_hilbert.h, nimcp_signal_filter.h

**Deliverables**:
- Phase-gated encoding/retrieval windows
- Four-tier memory with automatic promotion/decay

---

### Phase 5: SNN/Plasticity Integration

**Duration**: Neural substrate connection

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 5.1 | SNN Bridge | nimcp_pr_snn_bridge.h | nimcp_pr_snn_bridge.c | 2,500 |
| 5.2 | Plasticity Bridge | nimcp_pr_plasticity_bridge.h | nimcp_pr_plasticity_bridge.c | 3,000 |
| 5.3 | Pink Noise Bridge | nimcp_pr_pink_noise_bridge.h | nimcp_pr_pink_noise_bridge.c | 1,800 |

**Dependencies**: Phase 4, nimcp_snn.h, nimcp_plasticity_coordinator.h, nimcp_pink_noise.h

**Deliverables**:
- Quaternion → spike encoding (rate, burst, population, latency)
- STDP ↔ entanglement, BCM ↔ resonance threshold
- 1/f modulation of all memory dynamics

---

### Phase 6: Training Layer Integration

**Duration**: Learning system connection

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 6.1 | Loss Bridge | nimcp_pr_loss_bridge.h | nimcp_pr_loss_bridge.c | 1,500 |
| 6.2 | Optimizer Bridge | nimcp_pr_optimizer_bridge.h | nimcp_pr_optimizer_bridge.c | 1,500 |
| 6.3 | Meta-Learning Bridge | nimcp_pr_meta_bridge.h | nimcp_pr_meta_bridge.c | 1,500 |
| 6.4 | Curriculum Bridge | nimcp_pr_curriculum_bridge.h | nimcp_pr_curriculum_bridge.c | 1,500 |
| 6.5 | Continual Bridge | nimcp_pr_continual_bridge.h | nimcp_pr_continual_bridge.c | 1,500 |
| 6.6 | Training-Plasticity Ext | nimcp_pr_training_plasticity.h | nimcp_pr_training_plasticity.c | 1,500 |

**Dependencies**: Phase 5, nimcp_loss_functions.h, nimcp_optimizers.h, nimcp_meta_learning.h, etc.

**Deliverables**:
- Quaternion geodesic loss, resonance-triplet loss
- Resonance-aware Adam, quaternion momentum
- Memory-aware MAML, resonance-guided curriculum
- EWC + quaternion consolidation

---

### Phase 7: Perception Layer Integration

**Duration**: Sensory input connection

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 7.1 | Visual Bridge | nimcp_pr_visual_bridge.h | nimcp_pr_visual_bridge.c | 1,500 |
| 7.2 | Audio Bridge | nimcp_pr_audio_bridge.h | nimcp_pr_audio_bridge.c | 1,500 |
| 7.3 | Speech Bridge | nimcp_pr_speech_bridge.h | nimcp_pr_speech_bridge.c | 1,500 |
| 7.4 | Omni-Sensory Bridge | nimcp_pr_omni_bridge.h | nimcp_pr_omni_bridge.c | 1,500 |
| 7.5 | Attention Bridge | nimcp_pr_attention_bridge.h | nimcp_pr_attention_bridge.c | 1,500 |
| 7.6 | Predictive Bridge | nimcp_pr_predictive_bridge.h | nimcp_pr_predictive_bridge.c | 1,500 |

**Dependencies**: Phase 5, nimcp_visual_cortex.h, nimcp_audio_cortex.h, nimcp_speech_cortex.h, etc.

**Deliverables**:
- Visual features → prime signature + quaternion
- Audio/speech features → prime signature + quaternion
- Cross-modal fusion, attention × resonance gating
- FEP prediction error → resonance modulation

---

### Phase 8: Enhancement Layer - Part 1

**Duration**: Core enhancements

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 8.1 | Metamemory | nimcp_metamemory.h | nimcp_metamemory.c | 1,500 |
| 8.2 | Metamemory Monitoring | nimcp_metamemory_monitor.h | nimcp_metamemory_monitor.c | 1,500 |
| 8.3 | Prospective Memory | nimcp_prospective.h | nimcp_prospective.c | 2,000 |
| 8.4 | Prospective Scheduler | nimcp_prospective_scheduler.h | nimcp_prospective_scheduler.c | 1,500 |
| 8.5 | Reconsolidation | nimcp_reconsolidation.h | nimcp_reconsolidation.c | 2,000 |

**Deliverables**:
- Feeling of knowing, tip-of-tongue, judgments of learning
- Confidence calibration, knowledge inventory
- Intention nodes, time-based and event-based prospective memory
- Reconsolidation window, memory update mechanism

---

### Phase 9: Enhancement Layer - Part 2

**Duration**: Advanced enhancements

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 9.1 | Procedural Memory | nimcp_procedural.h | nimcp_procedural.c | 2,000 |
| 9.2 | Skill Acquisition | nimcp_skill_acquisition.h | nimcp_skill_acquisition.c | 2,000 |
| 9.3 | Future Thinking | nimcp_future_thinking.h | nimcp_future_thinking.c | 2,000 |
| 9.4 | Counterfactual | nimcp_counterfactual.h | nimcp_counterfactual.c | 1,500 |
| 9.5 | Source Memory | nimcp_source_memory.h | nimcp_source_memory.c | 2,000 |

**Deliverables**:
- Procedural memory system, skill stages (cognitive → associative → autonomous)
- Episodic future simulation, planning integration
- Counterfactual reasoning ("what if")
- Source attributes, reality monitoring, false memory detection

---

### Phase 10: Enhancement Layer - Part 3

**Duration**: Social and abstraction

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 10.1 | Social Memory | nimcp_social_memory.h | nimcp_social_memory.c | 2,000 |
| 10.2 | Collective Memory | nimcp_collective_memory.h | nimcp_collective_memory.c | 1,500 |
| 10.3 | Transactive Memory | nimcp_transactive.h | nimcp_transactive.c | 1,500 |
| 10.4 | Schema System | nimcp_schemas.h | nimcp_schemas.c | 2,000 |
| 10.5 | Gist Extraction | nimcp_gist.h | nimcp_gist.c | 2,000 |

**Deliverables**:
- Person nodes, relationship tracking, trust levels
- Multi-agent memory sync, cultural memory
- "Who knows what" knowledge
- Schema acquisition, gist extraction, schema-based inference

---

### Phase 11: Enhancement Layer - Part 4

**Duration**: Learning optimization and emotion

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 11.1 | Spaced Repetition | nimcp_spaced_repetition.h | nimcp_spaced_repetition.c | 2,000 |
| 11.2 | Flashbulb Memory | nimcp_flashbulb.h | nimcp_flashbulb.c | 2,000 |

**Deliverables**:
- Forgetting curve model, optimal review scheduling
- Retrieval practice effect, priority queue
- Enhanced emotional encoding, trauma handling
- Amygdala integration

---

### Phase 12: Module Bridges

**Duration**: Cross-system integration

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 12.1 | Bio-Async Bridge | nimcp_pr_bio_bridge.h | nimcp_pr_bio_bridge.c | 1,000 |
| 12.2 | Immune Bridge | nimcp_pr_immune_bridge.h | nimcp_pr_immune_bridge.c | 1,000 |
| 12.3 | Logging Bridge | nimcp_pr_logging_bridge.h | nimcp_pr_logging_bridge.c | 800 |
| 12.4 | Cognitive Bridge | nimcp_pr_cognitive_bridge.h | nimcp_pr_cognitive_bridge.c | 1,000 |
| 12.5 | Hypothalamus Bridge | nimcp_pr_hypo_bridge.h | nimcp_pr_hypo_bridge.c | 1,000 |
| 12.6 | Medulla Bridge | nimcp_pr_medulla_bridge.h | nimcp_pr_medulla_bridge.c | 1,000 |
| 12.7 | Cerebellum Bridge | nimcp_pr_cerebellum_bridge.h | nimcp_pr_cerebellum_bridge.c | 1,000 |
| 12.8 | Sleep-Wake Bridge | nimcp_pr_sleep_bridge.h | nimcp_pr_sleep_bridge.c | 1,200 |
| 12.9 | Mental Health Bridge | nimcp_pr_mental_health_bridge.h | nimcp_pr_mental_health_bridge.c | 1,000 |

**Deliverables**:
- Bidirectional integration with all major NIMCP modules
- Neuromodulator ↔ quaternion mapping
- Sleep consolidation integration
- Trauma/flashbulb ↔ mental health connection

---

### Phase 13: Master Integration

**Duration**: System controller

| ID | Component | Header | Source | Lines |
|----|-----------|--------|--------|-------|
| 13.1 | System Controller | nimcp_prime_resonant.h | nimcp_prime_resonant.c | 4,000 |

**Deliverables**:
- Unified API for all Prime Resonant functionality
- Lifecycle management
- Statistics and monitoring
- Configuration management

---

### Phase 14: Testing and Validation

**Duration**: Quality assurance

| ID | Component | Files | Lines |
|----|-----------|-------|-------|
| 14.1 | Unit Tests | tests/unit/cognitive/memory/test_*.c | 15,000 |
| 14.2 | Integration Tests | tests/integration/test_pr_*.c | 8,000 |
| 14.3 | Performance Tests | tests/performance/test_pr_perf_*.c | 3,000 |

---

## 5. Enhancement Specifications

### 5.1 Metamemory System

**Purpose**: Self-aware memory - knowing what you know

**Components**:

```c
// Feeling of Knowing
typedef struct {
    uint64_t memory_id;
    float fok_strength;        // 0-1, how strongly "I know this"
    float retrieval_likelihood; // Predicted success probability
    uint64_t partial_info;      // What we CAN retrieve
} fok_state_t;

// Judgment of Learning
typedef struct {
    uint64_t memory_id;
    float jol_rating;          // Predicted future recall (0-1)
    float encoding_strength;   // How well encoded
    float entanglement_density; // Support from related memories
    uint64_t timestamp;
} jol_t;

// Memory Confidence
typedef struct {
    float confidence;          // Subjective certainty (0-1)
    float calibration_error;   // Historical over/underconfidence
    bool verified;             // Externally confirmed?
} memory_confidence_t;
```

**API**:
```c
fok_state_t pr_get_feeling_of_knowing(pr_system_t* sys, prime_signature_t* query);
jol_t pr_judge_learning(pr_system_t* sys, uint64_t memory_id);
float pr_get_confidence(pr_system_t* sys, uint64_t memory_id);
void pr_update_calibration(pr_system_t* sys, uint64_t memory_id, bool was_correct);
pr_knowledge_inventory_t pr_get_knowledge_inventory(pr_system_t* sys, prime_signature_t* domain);
```

**Integration Points**:
- Training layer: JOL guides rehearsal decisions
- Cognitive layer: Confidence affects reasoning certainty
- Executive function: FOK triggers extended search

---

### 5.2 Prospective Memory System

**Purpose**: Remembering to do things in the future

**Components**:

```c
typedef enum {
    PROSPECTIVE_TIME_BASED,    // Trigger at specific time
    PROSPECTIVE_EVENT_BASED,   // Trigger on cue detection
    PROSPECTIVE_ACTIVITY_BASED // Trigger on activity completion
} prospective_type_t;

typedef struct {
    uint64_t intention_id;
    prospective_type_t type;
    prime_signature_t trigger_sig;  // What activates this
    prime_signature_t action_sig;   // What to do
    uint64_t deadline_ns;           // When (0 = no deadline)
    float urgency;                  // Priority (0-1)
    bool recurring;                 // Repeat after completion
    uint32_t recurrence_interval;   // If recurring, how often
    nimcp_quaternion_t state;       // Consolidation, importance
} prospective_memory_t;
```

**API**:
```c
uint64_t pr_create_intention(pr_system_t* sys, prospective_memory_t* intention);
bool pr_check_triggers(pr_system_t* sys, prime_signature_t* current_context);
prospective_memory_t* pr_get_triggered_intentions(pr_system_t* sys, size_t* count);
void pr_complete_intention(pr_system_t* sys, uint64_t intention_id);
void pr_cancel_intention(pr_system_t* sys, uint64_t intention_id);
```

**Integration Points**:
- Hypothalamus: Circadian timing for time-based triggers
- Perception: Current context for event-based triggers
- Executive function: Goal stack management

---

### 5.3 Memory Reconsolidation

**Purpose**: Memories that can be updated when retrieved

**State Machine**:
```
STABLE ──[retrieve]──► LABILE ──[6h timeout]──► STABLE
                          │                        ↑
                          └──[sleep consolidation]─┘
                          │
                          └──[new experience]──► MODIFIED
```

**Components**:

```c
typedef enum {
    MEMORY_STATE_STABLE,       // Normal state
    MEMORY_STATE_LABILE,       // Open for modification
    MEMORY_STATE_MODIFIED,     // Changed during labile window
    MEMORY_STATE_RECONSOLIDATING // Re-stabilizing
} memory_stability_t;

typedef struct {
    uint64_t memory_id;
    memory_stability_t state;
    uint64_t labile_start_ns;      // When became labile
    uint64_t labile_window_ns;     // How long window lasts (default 6h)
    nimcp_quaternion_t original;   // State before modification
    float modification_magnitude;  // How much changed
} reconsolidation_state_t;
```

**API**:
```c
void pr_mark_retrieved(pr_system_t* sys, uint64_t memory_id);
bool pr_is_labile(pr_system_t* sys, uint64_t memory_id);
void pr_update_labile_memory(pr_system_t* sys, uint64_t memory_id,
                             nimcp_quaternion_t* new_state);
void pr_reconsolidate(pr_system_t* sys, uint64_t memory_id);
```

**Integration Points**:
- Sleep bridge: Reconsolidation completes during sleep
- Mental health: Therapeutic modification of trauma memories
- Plasticity: STDP modulated by labile state

---

### 5.4 Procedural Memory System

**Purpose**: Skills, habits, and motor patterns

**Skill Stages**:
```
COGNITIVE ──► ASSOCIATIVE ──► AUTONOMOUS
   │              │               │
   │              │               └─ quat.w ≈ 1.0 (automatic)
   │              └─ Chunking, error reduction
   └─ Explicit steps, high attention
```

**Components**:

```c
typedef enum {
    SKILL_STAGE_COGNITIVE,     // Explicit, slow, effortful
    SKILL_STAGE_ASSOCIATIVE,   // Chunking, improving
    SKILL_STAGE_AUTONOMOUS     // Automatic, fast, parallel-capable
} skill_stage_t;

typedef struct {
    uint64_t skill_id;
    prime_signature_t trigger_context;  // When to activate
    prime_signature_t action_sequence;  // Compressed actions
    skill_stage_t stage;
    nimcp_quaternion_t state;
    // Quaternion meaning for procedural:
    // w = automaticity (0=effortful, 1=automatic)
    // x = reward history (positive/negative outcomes)
    // y = frequency (how often executed)
    // z = flexibility (can adapt to variations)
    uint64_t execution_count;
    float average_duration_ms;
    float error_rate;
} procedural_memory_t;

typedef struct {
    prime_signature_t cue;
    prime_signature_t routine;
    float habit_strength;      // 0-1, how automatic
    float reward_association;  // Expected reward
} habit_t;
```

**API**:
```c
uint64_t pr_learn_skill(pr_system_t* sys, prime_signature_t* actions, size_t step_count);
void pr_practice_skill(pr_system_t* sys, uint64_t skill_id, float performance);
skill_stage_t pr_get_skill_stage(pr_system_t* sys, uint64_t skill_id);
bool pr_can_execute_parallel(pr_system_t* sys, uint64_t skill_id);
uint64_t pr_form_habit(pr_system_t* sys, prime_signature_t* cue, prime_signature_t* routine);
float pr_get_habit_strength(pr_system_t* sys, uint64_t habit_id);
```

**Integration Points**:
- Cerebellum bridge: Motor skill coordination
- Striatum/dopamine: Habit formation reinforcement
- Training: Skill-based curriculum

---

### 5.5 Episodic Future Thinking

**Purpose**: Mental simulation of future scenarios

**Components**:

```c
typedef struct {
    prime_signature_t context;
    prime_signature_t location;
    prime_signature_t participants[8];
    size_t participant_count;
    uint64_t time_frame;           // Approximate when
    float vividness;               // Simulation quality
    nimcp_quaternion_t predicted_emotion; // Expected feelings
} future_simulation_request_t;

typedef struct {
    uint64_t simulation_id;
    pr_memory_node_t* simulated_episode; // Marked as IMAGINED
    float plausibility;            // How realistic
    float desirability;            // How good outcome
    prime_signature_t source_memories[16]; // What was combined
    size_t source_count;
} future_simulation_result_t;

typedef struct {
    uint64_t original_memory_id;
    prime_signature_t modification;  // What changed
    future_simulation_result_t alternative;
    float regret_intensity;          // If negative outcome
} counterfactual_t;
```

**API**:
```c
future_simulation_result_t* pr_simulate_future(pr_system_t* sys,
                                                future_simulation_request_t* req);
counterfactual_t* pr_simulate_counterfactual(pr_system_t* sys,
                                              uint64_t memory_id,
                                              prime_signature_t* modification);
float pr_evaluate_plan(pr_system_t* sys, prime_signature_t* action_sequence);
```

**Integration Points**:
- Decision-making: Evaluate options by simulating outcomes
- Imagination module: Shares simulation machinery
- Mental health: Worry/rumination detection

---

### 5.6 Source Memory

**Purpose**: Tracking where memories came from

**Components**:

```c
typedef enum {
    MEMORY_SOURCE_PERCEIVED,      // Directly experienced
    MEMORY_SOURCE_IMAGINED,       // Self-generated
    MEMORY_SOURCE_TOLD,           // Heard from others
    MEMORY_SOURCE_READ,           // From text/media
    MEMORY_SOURCE_INFERRED,       // Logically deduced
    MEMORY_SOURCE_COUNTERFACTUAL, // "What if" simulation
    MEMORY_SOURCE_DREAM,          // Sleep-generated
    MEMORY_SOURCE_UNKNOWN         // Source forgotten
} memory_source_t;

typedef struct {
    memory_source_t source;
    uint64_t encoding_time_ns;
    prime_signature_t encoding_context;  // Where learned
    float perceptual_detail;             // Sensory vividness (0-1)
    float cognitive_operations;          // Thought intensity
    float emotional_intensity;           // Emotion at encoding
    prime_signature_t informant;         // Who told me (if TOLD)
    float source_confidence;             // How sure of source
} source_memory_t;
```

**API**:
```c
source_memory_t* pr_get_source(pr_system_t* sys, uint64_t memory_id);
float pr_reality_monitor(pr_system_t* sys, uint64_t memory_id);
bool pr_is_source_confused(pr_system_t* sys, uint64_t memory_id);
float pr_get_reliability(pr_system_t* sys, uint64_t memory_id);
```

**Integration Points**:
- Reasoning: Source affects evidential weight
- Social cognition: Track who said what
- Epistemology: Support for belief revision

---

### 5.7 Social and Collective Memory

**Purpose**: Memory of people and shared knowledge

**Components**:

```c
// Person memory
typedef struct {
    uint64_t person_id;
    prime_signature_t identity_sig;     // Face, voice, name
    relationship_type_t relationship;   // Friend, family, etc.
    float trust_level;                  // 0-1
    nimcp_quaternion_t trait_impression;
    // w = familiarity
    // x = warmth (-1 cold to +1 warm)
    // y = competence (0-1)
    // z = status (0-1)
    uint64_t interaction_count;
    uint64_t last_interaction_ns;
} person_memory_t;

// Transactive memory
typedef struct {
    prime_signature_t domain;           // What knowledge
    uint64_t expert_person_id;          // Who knows
    float confidence;                   // How sure
    uint64_t last_verified_ns;          // When confirmed
} transactive_entry_t;

// Collective memory sync
typedef struct {
    uint64_t agent_id;
    prime_signature_t* shared_sigs;     // What to share
    size_t shared_count;
    bool include_private;               // Share private memories?
} collective_sync_request_t;
```

**API**:
```c
uint64_t pr_remember_person(pr_system_t* sys, person_memory_t* person);
void pr_update_person(pr_system_t* sys, uint64_t person_id, interaction_t* interaction);
float pr_get_trust(pr_system_t* sys, uint64_t person_id);
uint64_t pr_who_knows(pr_system_t* sys, prime_signature_t* domain);
void pr_sync_collective(pr_system_t* sys, collective_sync_request_t* req);
```

**Integration Points**:
- Social cognition module: Person understanding
- Collective cognition: Multi-agent coordination
- Theory of mind: Track others' knowledge

---

### 5.8 Schema System

**Purpose**: Abstract templates from specific experiences

**Components**:

```c
typedef struct {
    uint64_t schema_id;
    prime_signature_t abstract_sig;     // Generic signature
    char* name;                         // Human-readable name

    // Schema structure
    prime_signature_t* roles;           // Participants
    size_t role_count;
    prime_signature_t* sequence;        // Typical order
    size_t sequence_length;

    // Slot defaults
    struct {
        char* slot_name;
        prime_signature_t default_value;
        float confidence;
    } slots[32];
    size_t slot_count;

    // Hierarchy
    uint64_t parent_schema_id;          // Inherits from
    uint64_t* child_schema_ids;         // Specializations
    size_t child_count;

    // Statistics
    uint64_t instance_count;            // How many episodes match
    float abstraction_level;            // 0=specific, 1=abstract
} schema_t;
```

**API**:
```c
schema_t* pr_extract_schema(pr_system_t* sys, uint64_t* memory_ids, size_t count);
uint64_t* pr_find_schema_instances(pr_system_t* sys, uint64_t schema_id, size_t* count);
prime_signature_t pr_infer_from_schema(pr_system_t* sys, uint64_t schema_id,
                                        char* slot_name);
bool pr_check_schema_violation(pr_system_t* sys, uint64_t memory_id,
                                uint64_t schema_id, violation_t* out);
void pr_gist_extract(pr_system_t* sys, uint64_t memory_id);
```

**Integration Points**:
- Knowledge layer: Schema-based reasoning
- Perception: Schema-guided interpretation
- Learning: Schema acquisition during consolidation

---

### 5.9 Spaced Repetition

**Purpose**: Optimal memory maintenance

**Components**:

```c
typedef struct {
    uint64_t memory_id;
    float stability;           // Current retention strength
    float difficulty;          // Inherent difficulty (0-1)
    float ease_factor;         // Multiplier for interval growth
    uint64_t last_review_ns;
    uint64_t next_review_ns;
    uint32_t repetition_count;
    uint32_t lapse_count;      // Times forgotten
} srs_state_t;

typedef struct {
    uint64_t memory_id;
    bool recalled;             // Did retrieval succeed?
    float performance;         // Quality of recall (0-5)
    uint64_t response_time_ns; // How fast
} review_result_t;
```

**API**:
```c
srs_state_t* pr_get_srs_state(pr_system_t* sys, uint64_t memory_id);
void pr_record_review(pr_system_t* sys, review_result_t* result);
uint64_t* pr_get_due_reviews(pr_system_t* sys, uint64_t before_ns, size_t* count);
float pr_predict_recall_probability(pr_system_t* sys, uint64_t memory_id,
                                     uint64_t at_time_ns);
```

**Integration Points**:
- Training layer: Rehearsal scheduling
- Curriculum learning: Priority based on SRS state
- Sleep bridge: Review during sleep replay

---

### 5.10 Flashbulb Memory

**Purpose**: Enhanced encoding for emotional events

**Components**:

```c
typedef struct {
    pr_memory_node_t base;              // Standard memory
    float emotional_peak;               // Max emotion during event
    uint64_t duration_ns;               // Event duration
    prime_signature_t context_sig;      // Environmental snapshot
    float somatic_markers[8];           // Bodily state
    // [0] = heart_rate, [1] = skin_conductance, [2] = respiration,
    // [3] = muscle_tension, [4] = temperature, [5-7] = reserved
    bool is_traumatic;                  // Special handling flag
    uint64_t intrusion_count;           // Unwanted recalls
    float avoidance_strength;           // Suppression attempts
} flashbulb_memory_t;
```

**API**:
```c
bool pr_is_flashbulb_candidate(pr_system_t* sys, float emotional_intensity,
                                float novelty, float personal_significance);
uint64_t pr_encode_flashbulb(pr_system_t* sys, flashbulb_memory_t* memory);
void pr_mark_traumatic(pr_system_t* sys, uint64_t memory_id);
void pr_process_trauma(pr_system_t* sys, uint64_t memory_id,
                       trauma_therapy_t therapy_type);
```

**Integration Points**:
- Amygdala/emotional processing: Trigger detection
- Mental health bridge: Trauma processing
- Bio-async: Somatic marker capture
- Sleep bridge: Trauma replay management

---

## 6. Module Integration Updates

All existing NIMCP modules must be updated to leverage Prime Resonant capabilities.

### 6.1 Perception Modules

| Module | File | Integration |
|--------|------|-------------|
| Retina | nimcp_retina.h | Encode visual features → prime signature |
| Visual Cortex | nimcp_visual_cortex.h | Store visual memories via PR, top-down prediction |
| Cochlea | nimcp_cochlea.h | Encode audio features → prime signature |
| Audio Cortex | nimcp_audio_cortex.h | Store audio memories via PR, pattern matching |
| Speech Cortex | nimcp_speech_cortex.h | Phoneme sequences → prime signature, word recall |
| Omni-Sensory | nimcp_omni_sensory_bridge.h | Cross-modal binding via entanglement |

**Required Changes**:
```c
// In visual_cortex_process():
prime_signature_t sig = prime_sig_from_visual(features);
nimcp_quaternion_t quat = {
    .w = attention_peak,
    .x = emotional_valence,
    .y = novelty_score,
    .z = recognition_confidence
};
uint64_t mem_id = pr_encode(pr_sys, &sig, &quat, features, size);

// In visual_cortex_recall():
pr_memory_node_t* matches = pr_retrieve(pr_sys, &query_sig, k);
```

---

### 6.2 Cognitive Modules

| Module | File | Integration |
|--------|------|-------------|
| Attention | nimcp_attention.h | Salience → quat.y, attention modulates encoding |
| Working Memory | nimcp_working_memory.h | Use Z0 tier, 7±2 capacity |
| Reasoning | nimcp_reasoning.h | Query entanglement for associations, source memory for evidence |
| Executive | nimcp_executive.h | Prospective memory for goals, metamemory for planning |
| Imagination | nimcp_imagination.h | Share future thinking machinery, schema-based generation |
| Knowledge | nimcp_knowledge.h | Z3 tier for permanent facts, schema storage |
| Curiosity | nimcp_curiosity.h | Low resonance = novelty = explore |

**Required Changes**:
```c
// In working_memory_store():
// Replace custom buffer with Z0 tier
pr_store_z0(pr_sys, content, size);

// In reasoning_find_evidence():
// Query with source memory
source_memory_t* sources = pr_get_sources(pr_sys, evidence_ids, count);
for (i = 0; i < count; i++) {
    weight[i] *= sources[i].source_confidence;
}
```

---

### 6.3 Learning Modules

| Module | File | Integration |
|--------|------|-------------|
| STDP | nimcp_stdp.h | STDP updates → entanglement edges |
| BCM | nimcp_bcm.h | BCM threshold ↔ resonance threshold |
| Homeostatic | nimcp_homeostatic.h | Balance Z-ladder tier occupancy |
| Eligibility | nimcp_eligibility_trace.h | Eligibility → consolidation trigger |
| Meta-Learning | nimcp_meta_learning.h | Prime signature = task identity |
| Curriculum | nimcp_curriculum_learning.h | Resonance-based difficulty |
| Continual | nimcp_continual_learning.h | EWC + quaternion consolidation |

**Required Changes**:
```c
// In stdp_update():
if (weight_change > threshold) {
    pr_strengthen_entanglement(pr_sys, pre_memory_id, post_memory_id,
                               weight_change);
}

// In meta_learning_adapt():
// Store task experience
prime_signature_t task_sig = prime_sig_from_task(task);
pr_encode(pr_sys, &task_sig, &task_quat, adaptation_state, size);
```

---

### 6.4 Brain Region Modules

| Module | File | Integration |
|--------|------|-------------|
| Hippocampus | nimcp_hippocampus.h | Primary PR interface, all encoding/retrieval |
| Prefrontal | nimcp_prefrontal.h | Metamemory access, prospective memory |
| Amygdala | nimcp_amygdala.h | Flashbulb triggers, emotional tagging |
| Striatum | nimcp_striatum.h | Procedural memory, habit formation |
| Cerebellum | nimcp_cerebellum.h | Motor skill storage |
| Hypothalamus | nimcp_hypothalamus.h | Circadian timing for prospective memory |
| Medulla | nimcp_medulla.h | Arousal modulates encoding strength |

**Required Changes**:
```c
// In hippocampus_encode():
// Replace with PR encoding
uint64_t mem_id = pr_encode(pr_sys, &sig, &quat, content, size);
if (pr_is_flashbulb_candidate(pr_sys, emotional_intensity, novelty, significance)) {
    pr_encode_flashbulb(pr_sys, content);
}

// In amygdala_process():
// Trigger flashbulb encoding
if (emotional_intensity > FLASHBULB_THRESHOLD) {
    pr_trigger_flashbulb_mode(pr_sys);
}
```

---

### 6.5 Social/Emotional Modules

| Module | File | Integration |
|--------|------|-------------|
| Theory of Mind | nimcp_tom.h | Person memory, transactive memory |
| Social Cognition | nimcp_social.h | Relationship tracking, trust |
| Emotion | nimcp_emotion.h | Quaternion x-component, emotional memory |
| Empathy | nimcp_empathy.h | Person memory for empathic modeling |
| Mental Health | nimcp_mental_health.h | Trauma processing, reconsolidation therapy |

**Required Changes**:
```c
// In tom_model_other():
person_memory_t* person = pr_get_person(pr_sys, other_id);
// Use person's trait quaternion for prediction
predicted_action = infer_from_traits(person->trait_impression);

// In mental_health_process_trauma():
if (pr_is_labile(pr_sys, trauma_memory_id)) {
    pr_update_labile_memory(pr_sys, trauma_memory_id, &safer_quat);
}
```

---

### 6.6 Infrastructure Modules

| Module | File | Integration |
|--------|------|-------------|
| Bio-Async | nimcp_bio_async.h | Neuromodulator ↔ quaternion mapping |
| Sleep-Wake | nimcp_sleep_wake.h | Z-ladder consolidation, replay |
| Immune | nimcp_brain_immune.h | Stress affects encoding, inflammation markers |
| Logging | nimcp_logging.h | Memory statistics, retrieval logs |

**Required Changes**:
```c
// In bio_async_process_dopamine():
// Dopamine burst → boost consolidation
pr_modulate_quaternion_w(pr_sys, active_memories, dopamine_level);

// In sleep_consolidation():
// Promote Z1→Z2, Z2→Z3 based on resonance
pr_sleep_consolidate(pr_sys, sleep_stage);
// Replay high-resonance memories
pr_replay_memories(pr_sys, replay_list, count);
```

---

## 7. File Inventory

### 7.1 New Header Files (53 total)

```
include/cognitive/memory/
├── core/
│   ├── nimcp_quaternion.h
│   ├── nimcp_prime_signature.h
│   ├── nimcp_resonance.h
│   ├── nimcp_kuramoto.h
│   ├── nimcp_pr_memory_node.h
│   ├── nimcp_entanglement.h
│   ├── nimcp_z_ladder.h
│   └── nimcp_theta_gamma.h
│
├── enhancements/
│   ├── nimcp_metamemory.h
│   ├── nimcp_metamemory_monitor.h
│   ├── nimcp_prospective.h
│   ├── nimcp_prospective_scheduler.h
│   ├── nimcp_reconsolidation.h
│   ├── nimcp_procedural.h
│   ├── nimcp_skill_acquisition.h
│   ├── nimcp_future_thinking.h
│   ├── nimcp_counterfactual.h
│   ├── nimcp_source_memory.h
│   ├── nimcp_social_memory.h
│   ├── nimcp_collective_memory.h
│   ├── nimcp_transactive.h
│   ├── nimcp_schemas.h
│   ├── nimcp_gist.h
│   ├── nimcp_spaced_repetition.h
│   └── nimcp_flashbulb.h
│
├── bridges/
│   ├── nimcp_pr_snn_bridge.h
│   ├── nimcp_pr_plasticity_bridge.h
│   ├── nimcp_pr_pink_noise_bridge.h
│   ├── nimcp_pr_loss_bridge.h
│   ├── nimcp_pr_optimizer_bridge.h
│   ├── nimcp_pr_meta_bridge.h
│   ├── nimcp_pr_curriculum_bridge.h
│   ├── nimcp_pr_continual_bridge.h
│   ├── nimcp_pr_training_plasticity.h
│   ├── nimcp_pr_visual_bridge.h
│   ├── nimcp_pr_audio_bridge.h
│   ├── nimcp_pr_speech_bridge.h
│   ├── nimcp_pr_omni_bridge.h
│   ├── nimcp_pr_attention_bridge.h
│   ├── nimcp_pr_predictive_bridge.h
│   ├── nimcp_pr_bio_bridge.h
│   ├── nimcp_pr_immune_bridge.h
│   ├── nimcp_pr_logging_bridge.h
│   ├── nimcp_pr_cognitive_bridge.h
│   ├── nimcp_pr_hypo_bridge.h
│   ├── nimcp_pr_medulla_bridge.h
│   ├── nimcp_pr_cerebellum_bridge.h
│   ├── nimcp_pr_sleep_bridge.h
│   ├── nimcp_pr_mental_health_bridge.h
│   └── nimcp_pr_kg_bridge.h
│
├── utils/
│   ├── nimcp_pr_pink_noise.h
│   └── nimcp_fractal.h
│
└── nimcp_prime_resonant.h  (master header)
```

### 7.2 New Source Files (53 total)

Mirror structure in `src/cognitive/memory/`

### 7.3 Test Files

```
tests/
├── unit/cognitive/memory/
│   ├── test_quaternion.c
│   ├── test_prime_signature.c
│   ├── test_resonance.c
│   ├── test_kuramoto.c
│   ├── test_pr_memory_node.c
│   ├── test_entanglement.c
│   ├── test_z_ladder.c
│   ├── test_theta_gamma.c
│   ├── test_metamemory.c
│   ├── test_prospective.c
│   ├── test_reconsolidation.c
│   ├── test_procedural.c
│   ├── test_future_thinking.c
│   ├── test_source_memory.c
│   ├── test_social_memory.c
│   ├── test_schemas.c
│   ├── test_spaced_repetition.c
│   ├── test_flashbulb.c
│   └── test_prime_resonant.c
│
├── integration/
│   ├── test_pr_snn_integration.c
│   ├── test_pr_plasticity_integration.c
│   ├── test_pr_training_integration.c
│   ├── test_pr_perception_integration.c
│   ├── test_pr_cognitive_integration.c
│   └── test_pr_full_system.c
│
└── performance/
    ├── test_pr_perf_encoding.c
    ├── test_pr_perf_retrieval.c
    ├── test_pr_perf_consolidation.c
    └── test_pr_perf_scaling.c
```

---

## 8. Dependencies and Reuse

### 8.1 Existing Math Utilities (Reused)

| Utility | Purpose in PR |
|---------|---------------|
| nimcp_complex_math.h | Quaternion foundation, phase operations |
| nimcp_fft.h | Spectral shaping, 1/f verification |
| nimcp_signal_filter.h | Theta/gamma extraction |
| nimcp_hilbert.h | Envelope, phase, analytic signals |
| nimcp_integration.h | Kuramoto dynamics (RK4) |
| nimcp_tensor.h | Batch operations, SIMD |
| nimcp_svd_simple.h | Schema extraction (dimensionality reduction) |
| nimcp_kdtree.h | Spatial queries on entanglement graph |
| nimcp_quantum_walk.h | O(√N) memory search |

### 8.2 Existing Memory Utilities (Reused)

| Utility | Purpose in PR |
|---------|---------------|
| nimcp_cow_manager.h | Z2 tier storage, snapshot cloning |
| nimcp_unified_memory.h | Z3 tier storage, page-level COW |
| nimcp_memory_pool.h | Fast allocation for nodes/edges |

### 8.3 Existing Infrastructure (Reused)

| Module | Purpose in PR |
|--------|---------------|
| nimcp_pink_noise.h | 1/f modulation (extend, not replace) |
| nimcp_brain_kg.h | Knowledge graph integration |
| nimcp_plasticity_coordinator.h | Register PR with plasticity system |
| nimcp_bio_async.h | Neuromodulator messaging |

---

## 9. Testing Strategy

### 9.1 Unit Testing

Each component tested in isolation:

| Component | Key Tests |
|-----------|-----------|
| Quaternion | Hamilton product, SLERP, normalize, geodesic distance |
| Prime Signature | Generation, similarity, composition, hash collision rate |
| Resonance | Score computation, component weights, pink noise modulation |
| Kuramoto | Synchronization convergence, frequency modulation |
| Z-Ladder | Promotion rules, decay rates, capacity limits |
| Theta-Gamma | Phase gating, encoding windows, retrieval windows |
| Metamemory | FOK accuracy, JOL calibration, confidence tracking |
| Prospective | Trigger detection, deadline handling, recurring intentions |
| Procedural | Skill stages, automaticity, habit formation |

### 9.2 Integration Testing

Cross-component interactions:

| Test | Components |
|------|------------|
| Encode-Retrieve | Perception → PR → Retrieval |
| Consolidation | Z-Ladder + Sleep + STDP |
| Emotional Encoding | Flashbulb + Amygdala + Bio-Async |
| Social Memory | Person nodes + ToM + Social Cognition |
| Skill Learning | Procedural + Cerebellum + Training |

### 9.3 Performance Testing

| Metric | Target |
|--------|--------|
| Encoding latency | < 1ms for typical memory |
| Retrieval latency | < 10ms for top-10 results |
| Consolidation throughput | 1000 memories/second |
| Memory efficiency | < 1KB overhead per memory |
| COW savings | > 5x for shared templates |

### 9.4 Validation Metrics

| Capability | Validation |
|------------|------------|
| Content-addressable | Retrieve by partial cue with >90% accuracy |
| Forgetting curve | Match Ebbinghaus curve within 10% |
| Emotional enhancement | Flashbulb memories 2x more persistent |
| Schema extraction | Correctly identify common structure |
| Spaced repetition | Retention matches SM-2 predictions |

---

## 10. Risk Assessment

### 10.1 Technical Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Quaternion numerical instability | Medium | High | Regular normalization, epsilon guards |
| Prime signature collision | Low | Medium | Use 64 primes, collision detection |
| Resonance computation cost | Medium | Medium | Caching, approximate methods |
| Z-ladder memory bloat | Medium | High | Strict promotion criteria, aggressive decay |
| Entanglement graph explosion | Medium | High | Pruning old edges, max edge limits |

### 10.2 Integration Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Breaking existing modules | High | High | Incremental integration, feature flags |
| Performance regression | Medium | High | Continuous benchmarking |
| API incompatibility | Medium | Medium | Stable public API, internal versioning |

### 10.3 Scope Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Feature creep | High | Medium | Strict phase boundaries |
| Underestimated complexity | Medium | High | Buffer time in schedule |
| Testing inadequacy | Medium | High | Test-driven development |

---

## Appendix A: Message Types

New bio-async message types for Prime Resonant:

```c
// Memory events (0x1B00 - 0x1BFF)
#define BIO_MSG_PR_ENCODE           0x1B00
#define BIO_MSG_PR_RETRIEVE         0x1B01
#define BIO_MSG_PR_CONSOLIDATE      0x1B02
#define BIO_MSG_PR_FORGET           0x1B03
#define BIO_MSG_PR_ENTANGLE         0x1B04
#define BIO_MSG_PR_RESONANCE_UPDATE 0x1B05
#define BIO_MSG_PR_TIER_PROMOTE     0x1B06
#define BIO_MSG_PR_TIER_DEMOTE      0x1B07

// Metamemory events (0x1B10 - 0x1B1F)
#define BIO_MSG_PR_FOK_TRIGGERED    0x1B10
#define BIO_MSG_PR_TOT_STATE        0x1B11
#define BIO_MSG_PR_JOL_UPDATE       0x1B12
#define BIO_MSG_PR_CONFIDENCE_CHANGE 0x1B13

// Prospective memory events (0x1B20 - 0x1B2F)
#define BIO_MSG_PR_INTENTION_CREATE 0x1B20
#define BIO_MSG_PR_INTENTION_TRIGGER 0x1B21
#define BIO_MSG_PR_INTENTION_COMPLETE 0x1B22
#define BIO_MSG_PR_INTENTION_EXPIRE 0x1B23

// Procedural memory events (0x1B30 - 0x1B3F)
#define BIO_MSG_PR_SKILL_ACQUIRE    0x1B30
#define BIO_MSG_PR_SKILL_PRACTICE   0x1B31
#define BIO_MSG_PR_SKILL_STAGE_CHANGE 0x1B32
#define BIO_MSG_PR_HABIT_FORM       0x1B33
#define BIO_MSG_PR_HABIT_TRIGGER    0x1B34

// Social memory events (0x1B40 - 0x1B4F)
#define BIO_MSG_PR_PERSON_ENCOUNTER 0x1B40
#define BIO_MSG_PR_PERSON_UPDATE    0x1B41
#define BIO_MSG_PR_TRUST_CHANGE     0x1B42
#define BIO_MSG_PR_COLLECTIVE_SYNC  0x1B43

// Emotional memory events (0x1B50 - 0x1B5F)
#define BIO_MSG_PR_FLASHBULB_TRIGGER 0x1B50
#define BIO_MSG_PR_TRAUMA_DETECTED  0x1B51
#define BIO_MSG_PR_RECONSOLIDATION_START 0x1B52
#define BIO_MSG_PR_RECONSOLIDATION_END 0x1B53
```

---

## Appendix B: Configuration Defaults

```c
// Prime Resonant default configuration
static const pr_config_t PR_DEFAULT_CONFIG = {
    // Prime signature
    .prime_sig_dim = 64,
    .prime_sig_hash_bits = 64,

    // Quaternion
    .quat_normalize_threshold = 0.001f,
    .quat_slerp_threshold = 0.9995f,

    // Resonance
    .resonance_weights = {0.3f, 0.2f, 0.3f, 0.2f}, // Jaccard, Phase, Quat, Kuramoto
    .resonance_threshold = 0.5f,

    // Z-Ladder
    .z0_capacity = 7,
    .z1_capacity = 100,
    .z2_capacity = 10000,
    .z0_decay_tau_ms = 5000,
    .z1_decay_tau_ms = 3600000,    // 1 hour
    .z2_decay_tau_ms = 86400000,   // 1 day

    // Theta-Gamma
    .theta_frequency_hz = 6.0f,
    .gamma_frequency_hz = 40.0f,
    .encoding_phase_start = 0.0f,
    .encoding_phase_end = 1.57f,   // π/2
    .retrieval_phase_start = 3.14f, // π
    .retrieval_phase_end = 4.71f,  // 3π/2

    // Pink noise
    .pink_noise_octaves = 16,
    .pink_noise_modulation_depth = 0.1f,

    // Kuramoto
    .kuramoto_coupling_strength = 0.5f,
    .kuramoto_num_oscillators = 10,

    // Enhancements
    .metamemory_enabled = true,
    .prospective_enabled = true,
    .procedural_enabled = true,
    .social_memory_enabled = true,
    .flashbulb_enabled = true,
    .spaced_repetition_enabled = true,

    // Reconsolidation
    .reconsolidation_window_ns = 21600000000000ULL, // 6 hours

    // Flashbulb
    .flashbulb_emotion_threshold = 0.8f,
    .flashbulb_novelty_threshold = 0.7f,

    // Spaced repetition
    .srs_initial_ease = 2.5f,
    .srs_min_ease = 1.3f,
    .srs_initial_interval_days = 1,
};
```

---

## Appendix C: Glossary

| Term | Definition |
|------|------------|
| **Prime Signature** | Content-derived unique identifier using prime factorization |
| **Quaternion State** | 4D semantic state vector (consolidation, emotion, salience, accessibility) |
| **Entanglement** | Automatic association between memories based on resonance |
| **Z-Ladder** | Four-tier memory system (working → short → long → permanent) |
| **Resonance Score** | Combined similarity metric (Jaccard + Phase + Quat + Kuramoto) |
| **Theta-Gamma Coupling** | Oscillatory gating of encoding/retrieval windows |
| **Metamemory** | Self-awareness of memory state (FOK, TOT, JOL) |
| **Prospective Memory** | Future-oriented memory (intentions, goals) |
| **Reconsolidation** | Memory modification window after retrieval |
| **Procedural Memory** | Skill and habit storage |
| **Flashbulb Memory** | Enhanced encoding for emotional events |
| **Schema** | Abstract template extracted from episodes |
| **Gist** | Semantic essence after detail decay |
| **Transactive Memory** | "Who knows what" knowledge |

---

*End of Prime Resonant Implementation Plan*
