# NIMCP True Extrapolation Architecture
## Adding Causal Reasoning, Analogical Transfer, and Compositional Generalization

**Version:** 1.0
**Date:** 2025-11-21
**Status:** Design & Implementation Specification
**Authors:** NIMCP Development Team

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Current Limitations](#2-current-limitations)
3. [Architectural Vision](#3-architectural-vision)
4. [System Architecture](#4-system-architecture)
5. [Core Modules](#5-core-modules)
6. [Integration Architecture](#6-integration-architecture)
7. [Implementation Phases](#7-implementation-phases)
8. [Detailed Module Specifications](#8-detailed-module-specifications)
9. [API Specifications](#9-api-specifications)
10. [Data Structures](#10-data-structures)
11. [Algorithms](#11-algorithms)
12. [Testing Strategy](#12-testing-strategy)
13. [Performance Requirements](#13-performance-requirements)
14. [Migration Path](#14-migration-path)
15. [References](#15-references)

---

## 1. Executive Summary

### 1.1 Purpose

This document specifies the architecture and implementation plan for enhancing NIMCP with **true extrapolation capabilities**. Current NIMCP excels at pattern recognition and interpolation within training distributions but lacks the ability to generalize to fundamentally novel situations.

### 1.2 Goals

- **Enable causal reasoning** beyond correlation
- **Support analogical transfer** across domains
- **Achieve compositional generalization** with finite primitives
- **Implement meta-learning** for rapid task adaptation
- **Generate novel abstractions** through concept formation
- **Maintain backward compatibility** with existing NIMCP applications

### 1.3 Key Metrics

| Capability | Current | Target | Measurement |
|------------|---------|--------|-------------|
| Domain Transfer | 15% | 70% | Cross-domain task accuracy |
| Novel Concept Formation | 5% | 60% | Zero-shot concept recognition |
| Causal Inference | 10% | 80% | Intervention prediction accuracy |
| Few-Shot Learning | 30% | 85% | Accuracy with <10 examples |
| Extrapolation Error | 45% | <20% | Out-of-distribution prediction error |

### 1.4 Timeline

- **Phase 1 (Foundation):** 6 months - Causal reasoning, compositional systems
- **Phase 2 (Core):** 6 months - Analogy, meta-learning, world models
- **Phase 3 (Advanced):** 12 months - Concept formation, program synthesis
- **Total:** 24 months to full capability

---

## 2. Current Limitations

### 2.1 Architecture Constraints

```
Current NIMCP Architecture:
┌─────────────────────────────────────────────────────────────┐
│                     Brain Instance                          │
├─────────────────────────────────────────────────────────────┤
│ Symbolic Logic (1000 predicates, 500 rules) - BOUNDED      │
│ Neural Network (Spiking) - PATTERN-BASED                   │
│ Learning: STDP, BCM, Eligibility Traces - CORRELATION      │
│ Knowledge: 200 facts - FINITE                               │
│ Reasoning: Forward/Backward Chaining - DEDUCTIVE ONLY      │
└─────────────────────────────────────────────────────────────┘
         │
         ▼
   [Training Distribution]
         │
         ├─→ Inside: High accuracy (interpolation) ✓
         └─→ Outside: Poor accuracy (extrapolation) ✗
```

### 2.2 Specific Limitations

1. **No Causal Model:**
   - Current: `confidence = co_occurrence / (co_occurrence + antecedent_only)`
   - Problem: Cannot distinguish causation from correlation

2. **Fixed Knowledge Base:**
   - Current: Max 1000 predicates, 500 rules, 200 facts
   - Problem: Cannot grow knowledge unboundedly

3. **Pattern-Only Learning:**
   - Current: STDP, BCM (Hebbian learning)
   - Problem: No compositional or relational learning

4. **No Cross-Domain Transfer:**
   - Current: Single-domain pattern matching
   - Problem: Cannot apply knowledge from one domain to another

5. **No Meta-Learning:**
   - Current: Fixed learning algorithms
   - Problem: Cannot learn to learn or adapt quickly

---

## 3. Architectural Vision

### 3.1 Target Architecture

```
Enhanced NIMCP Architecture:
┌───────────────────────────────────────────────────────────────────┐
│                     Extrapolation Layer                           │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────────┐          │
│  │ Causal      │  │ Analogical   │  │ Compositional  │          │
│  │ Reasoning   │  │ Transfer     │  │ Generalization │          │
│  └──────┬──────┘  └──────┬───────┘  └────────┬───────┘          │
│         │                 │                    │                   │
│         └─────────────────┴────────────────────┘                  │
│                           ▼                                        │
│  ┌────────────────────────────────────────────────────────┐      │
│  │         Meta-Learning & Adaptation Layer               │      │
│  │  • Task Adaptation  • Few-Shot Learning  • Transfer    │      │
│  └─────────────────────────┬──────────────────────────────┘      │
│                            │                                       │
├────────────────────────────┼───────────────────────────────────────┤
│                     Core Brain Instance                           │
│  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐          │
│  │ Enhanced     │  │ World Model  │  │ Concept       │          │
│  │ Symbolic     │  │ & Simulation │  │ Formation     │          │
│  │ Logic        │  └──────────────┘  └───────────────┘          │
│  │ • Predicate  │                                                 │
│  │   Invention  │  ┌──────────────┐  ┌───────────────┐          │
│  │ • Unbounded  │  │ Semantic     │  │ Curiosity &   │          │
│  │   KB         │  │ Memory       │  │ Exploration   │          │
│  └──────────────┘  └──────────────┘  └───────────────┘          │
│                                                                    │
│  ┌────────────────────────────────────────────────────────┐      │
│  │         Existing Neural Network (Spiking)              │      │
│  │  • STDP  • BCM  • Eligibility Traces  • Neuromodulation│     │
│  └────────────────────────────────────────────────────────┘      │
└───────────────────────────────────────────────────────────────────┘
         │
         ▼
   [Training Distribution + Novel Domains]
         │
         ├─→ Inside: High accuracy (interpolation) ✓
         └─→ Outside: Reasonable accuracy (extrapolation) ✓
```

### 3.2 Layered Architecture

**Layer 1: Neural Foundation** (existing)
- Spiking neural networks
- STDP, BCM learning
- Neuromodulation

**Layer 2: Symbolic Foundation** (existing, enhanced)
- First-order logic
- Forward/backward chaining
- **NEW:** Unbounded KB, predicate invention

**Layer 3: Reasoning Layer** (NEW)
- Causal reasoning
- Analogical transfer
- Compositional generalization

**Layer 4: Meta-Layer** (NEW)
- Meta-learning
- Few-shot adaptation
- World models

**Layer 5: Concept Layer** (NEW)
- Concept formation
- Semantic memory
- Program synthesis

---

## 4. System Architecture

### 4.1 Module Hierarchy

```
nimcp/
├── src/
│   ├── core/
│   │   ├── brain/
│   │   │   ├── nimcp_brain.c              [Enhanced with new subsystems]
│   │   │   └── nimcp_brain_extrapolation.c [NEW]
│   │   ├── learning/
│   │   │   ├── nimcp_meta_learning.c       [NEW]
│   │   │   └── nimcp_few_shot.c            [NEW]
│   │   └── reasoning/
│   │       ├── nimcp_causal_reasoning.c    [NEW]
│   │       ├── nimcp_analogical_reasoning.c[NEW]
│   │       └── nimcp_compositional.c       [NEW]
│   ├── cognitive/
│   │   ├── nimcp_symbolic_logic.c          [Enhanced]
│   │   ├── nimcp_concept_formation.c       [NEW]
│   │   ├── nimcp_world_model.c             [NEW]
│   │   ├── nimcp_semantic_memory.c         [NEW]
│   │   ├── nimcp_curiosity.c               [NEW]
│   │   └── nimcp_program_synthesis.c       [NEW]
│   └── include/
│       ├── core/
│       │   ├── reasoning/
│       │   │   ├── nimcp_causal_reasoning.h
│       │   │   ├── nimcp_analogical_reasoning.h
│       │   │   └── nimcp_compositional.h
│       │   └── learning/
│       │       ├── nimcp_meta_learning.h
│       │       └── nimcp_few_shot.h
│       └── cognitive/
│           ├── nimcp_concept_formation.h
│           ├── nimcp_world_model.h
│           ├── nimcp_semantic_memory.h
│           ├── nimcp_curiosity.h
│           └── nimcp_program_synthesis.h
├── docs/
│   └── architecture/
│       ├── EXTRAPOLATION_CAPABILITIES.md   [This document]
│       ├── CAUSAL_REASONING_SPEC.md        [NEW]
│       ├── ANALOGICAL_TRANSFER_SPEC.md     [NEW]
│       └── META_LEARNING_SPEC.md           [NEW]
└── test/
    ├── unit/core/reasoning/                [NEW]
    ├── unit/cognitive/concept_formation/   [NEW]
    ├── integration/extrapolation/          [NEW]
    └── regression/extrapolation/           [NEW]
```

### 4.2 Dependency Graph

```
                    ┌──────────────────┐
                    │  Brain Instance  │
                    └────────┬─────────┘
                             │
        ┌────────────────────┼────────────────────┐
        │                    │                    │
        ▼                    ▼                    ▼
┌───────────────┐    ┌──────────────┐    ┌──────────────┐
│ Causal        │    │ Analogical   │    │ Meta-Learning│
│ Reasoning     │    │ Reasoning    │    │              │
└───────┬───────┘    └──────┬───────┘    └──────┬───────┘
        │                   │                    │
        │                   └──────────┬─────────┘
        │                              │
        ▼                              ▼
┌───────────────┐            ┌──────────────────┐
│ World Model   │            │ Compositional    │
│               │◄───────────│ Generalization   │
└───────┬───────┘            └──────────────────┘
        │
        ▼
┌───────────────┐            ┌──────────────────┐
│ Enhanced      │◄───────────│ Concept          │
│ Symbolic Logic│            │ Formation        │
└───────────────┘            └──────────────────┘
        │
        ▼
┌───────────────┐
│ Neural Network│
│ (Spiking)     │
└───────────────┘
```

---

## 5. Core Modules

### 5.1 Module Overview

| Module | Purpose | Priority | Dependencies | LOC Est. |
|--------|---------|----------|--------------|----------|
| Causal Reasoning | Distinguish causation from correlation | P0 | World Model, Enhanced Logic | 2,500 |
| Compositional Generalization | Combine primitives systematically | P0 | Enhanced Logic, Concept Formation | 2,000 |
| Enhanced Symbolic Logic | Unbounded KB, predicate invention | P0 | Existing Symbolic Logic | 1,500 |
| Analogical Reasoning | Cross-domain transfer | P1 | Compositional, Concept Formation | 3,000 |
| Meta-Learning | Learn to learn | P1 | Existing Learning, Few-Shot | 2,500 |
| World Model | Mental simulation | P1 | Neural Network, Planning | 3,500 |
| Concept Formation | Novel concept generation | P2 | Compositional, Semantic Memory | 2,500 |
| Semantic Memory | Large-scale knowledge | P2 | Enhanced Logic, Graph DB | 2,000 |
| Curiosity Engine | Intrinsic motivation | P2 | World Model, Exploration | 1,500 |
| Program Synthesis | Learn programs from I/O | P3 | Compositional, Bayesian Inference | 4,000 |

**Total Estimated LOC:** ~25,000 new lines + ~5,000 enhanced lines

---

## 6. Integration Architecture

### 6.1 Brain Structure Enhancement

**Current brain_t structure:**
```c
struct brain_impl {
    neural_network_t* network;
    task_strategy_t* strategy;
    symbolic_logic_engine_t* logic_engine;
    knowledge_system_t* knowledge;
    // ... existing fields
};
```

**Enhanced brain_t structure:**
```c
struct brain_impl {
    // ========================================
    // EXISTING SUBSYSTEMS (unchanged)
    // ========================================
    neural_network_t* network;
    task_strategy_t* strategy;
    symbolic_logic_engine_t* logic_engine;
    knowledge_system_t* knowledge;
    brain_config_t config;
    // ... all existing fields ...

    // ========================================
    // NEW EXTRAPOLATION SUBSYSTEMS
    // ========================================

    // Core Reasoning Modules
    causal_reasoning_t* causal_reasoning;           // P0: Causal inference
    compositional_system_t* compositional_system;   // P0: Composition
    analogical_engine_t* analogical_engine;         // P1: Analogy

    // Meta-Learning & Adaptation
    meta_learner_t* meta_learner;                   // P1: Meta-learning
    task_adapter_t* task_adapter;                   // P1: Fast adaptation
    few_shot_learner_t* few_shot_learner;          // P1: Few-shot learning

    // World & Concepts
    world_model_t* world_model;                     // P1: Mental simulation
    concept_formation_t* concept_formation;         // P2: Novel concepts
    semantic_memory_t* semantic_memory;             // P2: Large-scale KB

    // Exploration & Synthesis
    curiosity_module_t* curiosity;                  // P2: Intrinsic motivation
    program_synthesizer_t* program_synthesizer;     // P3: Program induction

    // Configuration
    extrapolation_config_t extrapolation_config;    // NEW: Extrapolation settings
};
```

### 6.2 Initialization Flow

```c
brain_t brain_create_custom(const brain_config_t* config)
{
    // ========================================
    // EXISTING INITIALIZATION (unchanged)
    // ========================================
    brain_t brain = nimcp_brain_factory_allocate_brain();
    // ... existing initialization ...

    // ========================================
    // NEW EXTRAPOLATION INITIALIZATION
    // ========================================

    // Phase 1: Core Reasoning (P0)
    if (config->enable_causal_reasoning) {
        brain->causal_reasoning = causal_reasoning_create(
            brain,
            config->causal_reasoning_config
        );
        if (!brain->causal_reasoning) {
            goto cleanup_extrapolation;
        }
    }

    if (config->enable_compositional_generalization) {
        brain->compositional_system = compositional_system_create(
            brain,
            config->compositional_config
        );
        if (!brain->compositional_system) {
            goto cleanup_extrapolation;
        }
    }

    // Phase 2: Advanced Reasoning (P1)
    if (config->enable_analogical_reasoning) {
        brain->analogical_engine = analogical_engine_create(
            brain,
            config->analogical_config
        );
        if (!brain->analogical_engine) {
            goto cleanup_extrapolation;
        }
    }

    if (config->enable_meta_learning) {
        brain->meta_learner = meta_learner_create(
            brain,
            config->meta_learning_config
        );
        brain->task_adapter = task_adapter_create(brain);
        brain->few_shot_learner = few_shot_learner_create(brain);
    }

    if (config->enable_world_model) {
        brain->world_model = world_model_create(
            brain,
            config->world_model_config
        );
    }

    // Phase 3: Concept Formation (P2)
    if (config->enable_concept_formation) {
        brain->concept_formation = concept_formation_create(
            brain,
            config->concept_config
        );

        brain->semantic_memory = semantic_memory_create(
            brain,
            config->semantic_memory_config
        );
    }

    // Phase 4: Exploration (P2)
    if (config->enable_curiosity) {
        brain->curiosity = curiosity_module_create(
            brain,
            config->curiosity_config
        );
    }

    // Phase 5: Program Synthesis (P3)
    if (config->enable_program_synthesis) {
        brain->program_synthesizer = program_synthesizer_create(
            brain,
            config->program_synthesis_config
        );
    }

    return brain;

cleanup_extrapolation:
    // Cleanup all extrapolation modules
    extrapolation_cleanup(brain);
    return NULL;
}
```

### 6.3 Brain Config Extension

```c
typedef struct {
    // ========================================
    // EXISTING CONFIG FIELDS (unchanged)
    // ========================================
    char task_name[256];
    brain_size_t size;
    brain_task_t task;
    uint32_t num_inputs;
    uint32_t num_outputs;
    // ... all existing fields ...

    // ========================================
    // NEW EXTRAPOLATION CONFIG
    // ========================================

    // Enable/disable modules
    bool enable_causal_reasoning;
    bool enable_compositional_generalization;
    bool enable_analogical_reasoning;
    bool enable_meta_learning;
    bool enable_world_model;
    bool enable_concept_formation;
    bool enable_semantic_memory;
    bool enable_curiosity;
    bool enable_program_synthesis;

    // Module-specific configs
    causal_reasoning_config_t causal_reasoning_config;
    compositional_config_t compositional_config;
    analogical_config_t analogical_config;
    meta_learning_config_t meta_learning_config;
    world_model_config_t world_model_config;
    concept_config_t concept_config;
    semantic_memory_config_t semantic_memory_config;
    curiosity_config_t curiosity_config;
    program_synthesis_config_t program_synthesis_config;

} brain_config_t;
```

---

## 7. Implementation Phases

### 7.1 Phase 1: Foundation (Months 1-6)

**Goal:** Core infrastructure for causal reasoning and compositional generalization

#### Milestone 1.1: Enhanced Symbolic Logic (Month 1-2)
- **Deliverables:**
  - Unbounded knowledge base (remove 1000/500/200 limits)
  - Dynamic predicate creation
  - Predicate invention from observations
  - Enhanced unification with occurs-check

- **Implementation Tasks:**
  ```
  [ ] Replace fixed arrays with dynamic hash tables
  [ ] Implement predicate_invent() function
  [ ] Add predicate lifecycle management
  [ ] Update all symbolic logic tests
  [ ] Benchmark: 10,000+ predicates, <100ms query time
  ```

- **Success Criteria:**
  - Support 10,000+ predicates
  - Support 5,000+ rules
  - Support 10,000+ facts
  - Query time <100ms for 10K KB
  - Backward compatible with existing symbolic logic

#### Milestone 1.2: Causal Reasoning Engine (Month 2-4)
- **Deliverables:**
  - Causal graph representation (DAG)
  - Intervention calculus (do-operator)
  - Counterfactual inference
  - Causal discovery (PC algorithm)

- **Implementation Tasks:**
  ```
  [ ] Implement causal graph data structure
  [ ] Implement do-calculus (3 rules of Pearl)
  [ ] Implement counterfactual queries
  [ ] Implement PC algorithm for causal discovery
  [ ] Implement conditional independence tests
  [ ] Add causal strength estimation
  [ ] Create comprehensive test suite
  ```

- **Success Criteria:**
  - Correctly distinguish causation from correlation (95% accuracy on synthetic data)
  - Compute intervention effects P(Y|do(X))
  - Answer counterfactual queries
  - Discover causal structure from observational data

#### Milestone 1.3: Compositional Generalization (Month 4-6)
- **Deliverables:**
  - Compositional semantics engine
  - Primitive concept library
  - Composition operations
  - Systematic generalization tests

- **Implementation Tasks:**
  ```
  [ ] Design compositional algebra (operators: AND, OR, NOT, COMPOSE)
  [ ] Implement primitive concept library
  [ ] Implement composition functions
  [ ] Implement systematic generalization
  [ ] Test on SCAN-like benchmarks
  [ ] Integrate with symbolic logic
  ```

- **Success Criteria:**
  - Generate infinite expressions from finite primitives
  - Pass SCAN benchmark (>95% compositional split)
  - Generalize to novel combinations unseen in training

### 7.2 Phase 2: Core Capabilities (Months 7-12)

#### Milestone 2.1: Analogical Reasoning (Month 7-8)
- **Deliverables:**
  - Structure mapping engine (SME)
  - Analogical transfer mechanism
  - Cross-domain mapping

- **Implementation Tasks:**
  ```
  [ ] Implement structure mapping algorithm
  [ ] Implement analogical distance metric
  [ ] Implement transfer by structural similarity
  [ ] Test on analogy datasets (e.g., Greek:Athens::France:?)
  [ ] Integrate with compositional system
  ```

- **Success Criteria:**
  - Solve analogy problems (A:B::C:?)
  - Transfer knowledge across domains (70% accuracy)
  - Map relational structures

#### Milestone 2.2: Meta-Learning (Month 8-10)
- **Deliverables:**
  - MAML or Reptile implementation
  - Few-shot learning interface
  - Task adaptation mechanism

- **Implementation Tasks:**
  ```
  [ ] Implement MAML algorithm
  [ ] Implement task distribution sampler
  [ ] Implement few-shot adaptation
  [ ] Test on Omniglot, Mini-ImageNet
  [ ] Integrate with existing learning
  ```

- **Success Criteria:**
  - <10 examples to adapt to new task
  - >80% accuracy after adaptation
  - <1 second adaptation time

#### Milestone 2.3: World Model (Month 10-12)
- **Deliverables:**
  - Forward dynamics model
  - Reward prediction model
  - Mental simulation engine
  - Model-based planning

- **Implementation Tasks:**
  ```
  [ ] Implement transition model s' = f(s,a)
  [ ] Implement reward model r = r(s,a)
  [ ] Implement uncertainty estimation
  [ ] Implement trajectory simulation
  [ ] Implement model-based planning (MCTS or similar)
  [ ] Test on control tasks
  ```

- **Success Criteria:**
  - Predict future states (90% accuracy 1-step, 70% 10-step)
  - Plan using imagination
  - Sample-efficient learning (10x improvement)

### 7.3 Phase 3: Advanced Features (Months 13-24)

#### Milestone 3.1: Concept Formation (Month 13-16)
- **Deliverables:**
  - Conceptual blending
  - Metaphor generation
  - Schema induction

- **Implementation Tasks:**
  ```
  [ ] Implement conceptual spaces
  [ ] Implement blending algorithm (Fauconnier & Turner)
  [ ] Implement metaphor mapping
  [ ] Implement schema abstraction
  [ ] Test on creative tasks
  ```

- **Success Criteria:**
  - Generate novel concepts via blending
  - Produce meaningful metaphors
  - Abstract schemas from instances

#### Milestone 3.2: Semantic Memory (Month 16-18)
- **Deliverables:**
  - Large-scale knowledge graph
  - Common sense reasoning
  - Semantic embeddings

- **Implementation Tasks:**
  ```
  [ ] Integrate ConceptNet or similar KB
  [ ] Implement knowledge graph queries
  [ ] Implement semantic similarity
  [ ] Implement common sense inference
  [ ] Test on ATOMIC, Social IQA
  ```

- **Success Criteria:**
  - 1M+ facts
  - Query time <50ms
  - Common sense accuracy >70%

#### Milestone 3.3: Curiosity & Exploration (Month 18-20)
- **Deliverables:**
  - Novelty detection
  - Intrinsic motivation
  - Active learning

- **Implementation Tasks:**
  ```
  [ ] Implement ICM or RND
  [ ] Implement curiosity-driven rewards
  [ ] Implement active query selection
  [ ] Test on exploration tasks
  ```

- **Success Criteria:**
  - Explore novel states
  - Request informative labels
  - Improve sample efficiency

#### Milestone 3.4: Program Synthesis (Month 20-24)
- **Deliverables:**
  - Domain-specific language
  - Synthesis from I/O examples
  - Verification engine

- **Implementation Tasks:**
  ```
  [ ] Design DSL for programs
  [ ] Implement enumerative search
  [ ] Implement neural-guided search
  [ ] Implement program verification
  [ ] Test on program synthesis benchmarks
  ```

- **Success Criteria:**
  - Synthesize programs from I/O examples
  - Handle loops, conditionals, recursion
  - Verify correctness

---

## 8. Detailed Module Specifications

### 8.1 Causal Reasoning Module

#### 8.1.1 Header: `include/core/reasoning/nimcp_causal_reasoning.h`

```c
/**
 * @file nimcp_causal_reasoning.h
 * @brief Causal reasoning and inference
 *
 * Implements causal graphs, intervention calculus, and counterfactual reasoning
 * based on Judea Pearl's causal hierarchy (Pearl, 2009).
 *
 * Capabilities:
 * - Build causal DAG from observational data
 * - Compute intervention effects P(Y|do(X=x))
 * - Answer counterfactual queries "What if...?"
 * - Distinguish causation from correlation
 *
 * References:
 * - Pearl, J. (2009). Causality: Models, Reasoning, and Inference
 * - Spirtes, Glymour, Scheines (2000). Causation, Prediction, and Search
 */

#ifndef NIMCP_CAUSAL_REASONING_H
#define NIMCP_CAUSAL_REASONING_H

#include "core/brain/nimcp_brain.h"
#include "core/graph/nimcp_graph.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Type Definitions
// ============================================================================

/**
 * @brief Causal graph node representing a variable
 */
typedef struct {
    char name[64];                    // Variable name
    uint32_t node_id;                 // Unique node ID
    float* values;                    // Observed values
    uint32_t num_values;              // Number of observations
    void* distribution;               // Probability distribution
} causal_node_t;

/**
 * @brief Causal edge representing direct causal relationship
 */
typedef struct {
    uint32_t from;                    // Parent node ID
    uint32_t to;                      // Child node ID
    float causal_strength;            // Estimated causal effect
    float confidence;                 // Confidence in edge (0-1)
    char mechanism[256];              // Causal mechanism description
} causal_edge_t;

/**
 * @brief Causal graph (Directed Acyclic Graph)
 */
typedef struct {
    causal_node_t** nodes;            // Array of nodes
    uint32_t num_nodes;               // Number of nodes
    uint32_t capacity_nodes;          // Capacity

    causal_edge_t** edges;            // Array of edges
    uint32_t num_edges;               // Number of edges
    uint32_t capacity_edges;          // Capacity

    graph_t* topology;                // Underlying graph structure
} causal_graph_t;

/**
 * @brief Intervention - setting a variable to a specific value
 */
typedef struct {
    char variable[64];                // Variable to intervene on
    float value;                      // Value to set
    bool is_range;                    // Whether value is a range
    float range_min;                  // Range minimum (if is_range)
    float range_max;                  // Range maximum (if is_range)
} intervention_t;

/**
 * @brief Counterfactual query
 */
typedef struct {
    char query_var[64];               // Variable to query
    intervention_t* interventions;    // Array of interventions
    uint32_t num_interventions;       // Number of interventions
    float* observed_values;           // Actual observed values
    uint32_t num_observed;            // Number of observations
} counterfactual_query_t;

/**
 * @brief Causal discovery algorithm
 */
typedef enum {
    CAUSAL_DISCOVERY_PC,              // PC algorithm (constraint-based)
    CAUSAL_DISCOVERY_GES,             // Greedy Equivalence Search (score-based)
    CAUSAL_DISCOVERY_FCI,             // Fast Causal Inference (with latent variables)
    CAUSAL_DISCOVERY_LiNGAM,          // Linear Non-Gaussian Acyclic Model
    CAUSAL_DISCOVERY_NOTEARS          // Non-combinatorial optimization
} causal_discovery_algorithm_t;

/**
 * @brief Causal reasoning configuration
 */
typedef struct {
    causal_discovery_algorithm_t discovery_algorithm;
    float independence_threshold;     // p-value threshold for independence tests
    float edge_confidence_threshold;  // Minimum confidence to include edge
    uint32_t max_parents;             // Maximum parents per node
    bool allow_latent_confounders;    // Whether to model hidden confounders
    bool learn_mechanisms;            // Whether to learn causal mechanisms
} causal_reasoning_config_t;

/**
 * @brief Causal reasoning engine
 */
typedef struct {
    brain_t brain;                    // Parent brain
    causal_graph_t* causal_graph;     // Learned causal graph
    causal_reasoning_config_t config; // Configuration

    // Cached computations
    void* intervention_cache;         // Cache intervention results
    void* counterfactual_cache;       // Cache counterfactual results
} causal_reasoning_t;

// ============================================================================
// Core Functions
// ============================================================================

/**
 * @brief Create causal reasoning engine
 *
 * @param brain Parent brain instance
 * @param config Configuration
 * @return causal_reasoning_t* Causal reasoning engine, NULL on failure
 */
causal_reasoning_t* causal_reasoning_create(
    brain_t brain,
    const causal_reasoning_config_t* config
);

/**
 * @brief Destroy causal reasoning engine
 *
 * @param cr Causal reasoning engine
 */
void causal_reasoning_destroy(causal_reasoning_t* cr);

// ============================================================================
// Causal Graph Construction
// ============================================================================

/**
 * @brief Add variable to causal graph
 *
 * @param cr Causal reasoning engine
 * @param name Variable name
 * @return uint32_t Node ID, UINT32_MAX on failure
 */
uint32_t causal_graph_add_variable(
    causal_reasoning_t* cr,
    const char* name
);

/**
 * @brief Add causal edge to graph
 *
 * @param cr Causal reasoning engine
 * @param from Parent variable name
 * @param to Child variable name
 * @param strength Causal strength
 * @param confidence Confidence in edge
 * @return bool True on success
 */
bool causal_graph_add_edge(
    causal_reasoning_t* cr,
    const char* from,
    const char* to,
    float strength,
    float confidence
);

/**
 * @brief Learn causal graph from observational data
 *
 * Uses configured discovery algorithm (PC, GES, etc.) to learn
 * causal structure from joint observations.
 *
 * @param cr Causal reasoning engine
 * @param variable_names Array of variable names
 * @param num_variables Number of variables
 * @param data Observation matrix [num_samples x num_variables]
 * @param num_samples Number of samples
 * @return bool True if causal graph learned successfully
 *
 * @complexity O(V^2 * N) for PC algorithm where V=variables, N=samples
 */
bool causal_graph_learn_from_data(
    causal_reasoning_t* cr,
    const char** variable_names,
    uint32_t num_variables,
    float** data,
    uint32_t num_samples
);

// ============================================================================
// Causal Inference
// ============================================================================

/**
 * @brief Test if X causes Y
 *
 * Checks if there is a directed path from X to Y in the causal graph.
 *
 * @param cr Causal reasoning engine
 * @param cause Potential cause variable
 * @param effect Potential effect variable
 * @return bool True if X causes Y
 */
bool causal_is_cause(
    causal_reasoning_t* cr,
    const char* cause,
    const char* effect
);

/**
 * @brief Compute causal effect strength
 *
 * Estimates the average causal effect (ACE) of X on Y.
 * ACE = E[Y|do(X=1)] - E[Y|do(X=0)]
 *
 * @param cr Causal reasoning engine
 * @param cause Cause variable
 * @param effect Effect variable
 * @param ace Output: average causal effect
 * @return bool True on success
 */
bool causal_compute_effect_strength(
    causal_reasoning_t* cr,
    const char* cause,
    const char* effect,
    float* ace
);

/**
 * @brief Identify causal effect (Pearl's do-calculus)
 *
 * Determines if P(Y|do(X)) is identifiable from P(V) where V are
 * observed variables, using Pearl's do-calculus rules.
 *
 * @param cr Causal reasoning engine
 * @param intervention Intervention variable
 * @param outcome Outcome variable
 * @param identifiable Output: whether effect is identifiable
 * @return bool True on success
 */
bool causal_identify_effect(
    causal_reasoning_t* cr,
    const char* intervention,
    const char* outcome,
    bool* identifiable
);

// ============================================================================
// Intervention Calculus
// ============================================================================

/**
 * @brief Compute intervention effect P(Y|do(X=x))
 *
 * Computes the interventional distribution using the truncated factorization:
 * P(V|do(X=x)) = ∏_{V_i ≠ X} P(V_i|PA_i) where PA_i are parents of V_i
 *
 * @param cr Causal reasoning engine
 * @param intervention Intervention specification
 * @param outcome_var Outcome variable to query
 * @param outcome_dist Output: distribution over outcome (caller allocates)
 * @param dist_size Size of outcome_dist array
 * @return bool True on success
 *
 * @complexity O(V * 2^k) where V=variables, k=max_parents
 */
bool causal_compute_intervention(
    causal_reasoning_t* cr,
    const intervention_t* intervention,
    const char* outcome_var,
    float* outcome_dist,
    uint32_t dist_size
);

/**
 * @brief Compute conditional intervention P(Y|do(X=x), Z=z)
 *
 * @param cr Causal reasoning engine
 * @param intervention Intervention specification
 * @param conditions Array of conditioning variables and values
 * @param num_conditions Number of conditions
 * @param outcome_var Outcome variable
 * @param outcome_dist Output distribution
 * @param dist_size Size of outcome_dist
 * @return bool True on success
 */
bool causal_compute_conditional_intervention(
    causal_reasoning_t* cr,
    const intervention_t* intervention,
    const intervention_t* conditions,
    uint32_t num_conditions,
    const char* outcome_var,
    float* outcome_dist,
    uint32_t dist_size
);

// ============================================================================
// Counterfactual Reasoning
// ============================================================================

/**
 * @brief Answer counterfactual query
 *
 * Computes P(Y_x|X'=x', Y'=y') where:
 * - Y_x is the value Y would have if X were set to x
 * - X'=x', Y'=y' are the actual observed values
 *
 * This requires solving the structural equations with exogenous
 * variables set to match the observations.
 *
 * @param cr Causal reasoning engine
 * @param query Counterfactual query specification
 * @param result Output: counterfactual value
 * @return bool True on success
 *
 * @complexity O(V * 2^k) where V=variables, k=max_parents
 */
bool causal_compute_counterfactual(
    causal_reasoning_t* cr,
    const counterfactual_query_t* query,
    float* result
);

/**
 * @brief Compute probability of necessity (PN)
 *
 * PN = P(Y_x=0|X=1, Y=1) = probability that X=1 was necessary for Y=1
 *
 * @param cr Causal reasoning engine
 * @param cause Cause variable
 * @param effect Effect variable
 * @param pn Output: probability of necessity
 * @return bool True on success
 */
bool causal_probability_of_necessity(
    causal_reasoning_t* cr,
    const char* cause,
    const char* effect,
    float* pn
);

/**
 * @brief Compute probability of sufficiency (PS)
 *
 * PS = P(Y_x=1|X=0, Y=0) = probability that X=1 is sufficient for Y=1
 *
 * @param cr Causal reasoning engine
 * @param cause Cause variable
 * @param effect Effect variable
 * @param ps Output: probability of sufficiency
 * @return bool True on success
 */
bool causal_probability_of_sufficiency(
    causal_reasoning_t* cr,
    const char* cause,
    const char* effect,
    float* ps
);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Test conditional independence
 *
 * Tests if X ⊥ Y | Z using chi-squared test or G-test
 *
 * @param data Observation matrix
 * @param num_samples Number of samples
 * @param x_idx Index of X variable
 * @param y_idx Index of Y variable
 * @param z_indices Indices of Z variables (conditioning set)
 * @param num_z Number of Z variables
 * @param p_value Output: p-value of independence test
 * @return bool True if independent (p_value > threshold)
 */
bool causal_test_independence(
    float** data,
    uint32_t num_samples,
    uint32_t x_idx,
    uint32_t y_idx,
    uint32_t* z_indices,
    uint32_t num_z,
    float* p_value
);

/**
 * @brief Get causal parents of variable
 *
 * @param cr Causal reasoning engine
 * @param variable Variable name
 * @param parents Output: array of parent names (caller frees)
 * @param num_parents Output: number of parents
 * @return bool True on success
 */
bool causal_get_parents(
    causal_reasoning_t* cr,
    const char* variable,
    char*** parents,
    uint32_t* num_parents
);

/**
 * @brief Get causal children of variable
 *
 * @param cr Causal reasoning engine
 * @param variable Variable name
 * @param children Output: array of child names (caller frees)
 * @param num_children Output: number of children
 * @return bool True on success
 */
bool causal_get_children(
    causal_reasoning_t* cr,
    const char* variable,
    char*** children,
    uint32_t* num_children
);

/**
 * @brief Export causal graph to DOT format
 *
 * @param cr Causal reasoning engine
 * @param filename Output file path
 * @return bool True on success
 */
bool causal_export_graph_dot(
    causal_reasoning_t* cr,
    const char* filename
);

/**
 * @brief Print causal graph summary
 *
 * @param cr Causal reasoning engine
 */
void causal_print_graph(const causal_reasoning_t* cr);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CAUSAL_REASONING_H
```

#### 8.1.2 Implementation Notes

**Algorithm: PC (Peter-Clark) Algorithm for Causal Discovery**

```
Input: Joint observations D = {X1, X2, ..., Xn}
Output: Causal DAG G

1. Start with complete undirected graph
2. For each pair (Xi, Xj):
   - Test Xi ⊥ Xj | Z for all subsets Z ⊂ V \ {Xi, Xj}
   - If independent, remove edge Xi -- Xj
3. Orient edges using collider detection:
   - If Xi -- Z -- Xj and Xi, Xj not adjacent, orient Xi → Z ← Xj
4. Orient remaining edges using orientation rules:
   - R1: Xi → Xj -- Xk, Xi, Xk not adjacent → Xi → Xj → Xk
   - R2: Xi → Xj → Xk, Xi -- Xk → Xi → Xk
   - R3: Complex chain rules
5. Return oriented DAG

Complexity: O(V^2 * 2^k * N) where k = max conditioning set size
```

**Do-Calculus Rules (Pearl, 1995):**

```
Rule 1 (Insertion/deletion of observations):
  P(y|do(x),z,w) = P(y|do(x),w) if (Y ⊥ Z | X,W) in G_X̅

Rule 2 (Action/observation exchange):
  P(y|do(x),do(z),w) = P(y|do(x),z,w) if (Y ⊥ Z | X,W) in G_X̅Z

Rule 3 (Insertion/deletion of actions):
  P(y|do(x),do(z),w) = P(y|do(x),w) if (Y ⊥ Z | X,W) in G_X̅Z(W)

Where:
- G_X̅ = graph with all arrows into X removed
- G_X̅Z = graph with arrows into X removed, arrows from Z removed
```

**Counterfactual Computation (3-step process):**

```
1. Abduction: Compute P(U|X=x, Y=y) from observed data
2. Action: Replace equation for X with X=x'
3. Prediction: Compute P(Y|U, do(X=x'))

Example:
Observed: Aspirin=1, Headache=0
Query: Would headache occur if no aspirin? Y_{aspirin=0}

Step 1: Solve for exogenous U given Aspirin=1, Headache=0
Step 2: Set Aspirin=0
Step 3: Compute Headache with Aspirin=0 and inferred U
```

---

### 8.2 Compositional Generalization Module

#### 8.2.1 Header: `include/core/reasoning/nimcp_compositional.h`

```c
/**
 * @file nimcp_compositional.h
 * @brief Compositional generalization and systematic reasoning
 *
 * Enables the brain to combine primitive concepts systematically to
 * generate and understand infinite novel expressions from finite primitives.
 *
 * Capabilities:
 * - Compositional semantics (Montague grammar)
 * - Systematic generalization
 * - Productive reasoning
 *
 * References:
 * - Fodor & Pylyshyn (1988). Connectionism and cognitive architecture
 * - Lake & Baroni (2018). Generalization without systematicity
 */

#ifndef NIMCP_COMPOSITIONAL_H
#define NIMCP_COMPOSITIONAL_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Type Definitions
// ============================================================================

/**
 * @brief Primitive concept types
 */
typedef enum {
    CONCEPT_ENTITY,                   // Objects (e.g., "dog", "car")
    CONCEPT_PROPERTY,                 // Attributes (e.g., "red", "fast")
    CONCEPT_RELATION,                 // Relations (e.g., "on", "loves")
    CONCEPT_ACTION,                   // Actions (e.g., "run", "eat")
    CONCEPT_MODIFIER,                 // Modifiers (e.g., "very", "not")
    CONCEPT_FUNCTION,                 // Functions (e.g., "father_of")
    CONCEPT_QUANTIFIER                // Quantifiers (e.g., "all", "some")
} concept_type_t;

/**
 * @brief Composition operation types
 */
typedef enum {
    COMPOSE_APPLICATION,              // Function application: f(x)
    COMPOSE_CONJUNCTION,              // Conjunction: A AND B
    COMPOSE_DISJUNCTION,              // Disjunction: A OR B
    COMPOSE_NEGATION,                 // Negation: NOT A
    COMPOSE_MODIFICATION,             // Modification: ADJ NOUN
    COMPOSE_PREDICATION,              // Predication: VERB(SUBJ, OBJ)
    COMPOSE_QUANTIFICATION,           // Quantification: QUANT(VAR, PRED)
    COMPOSE_ABSTRACTION               // Lambda abstraction: λx.P(x)
} composition_op_t;

/**
 * @brief Primitive concept
 */
typedef struct {
    char name[128];                   // Concept name
    concept_type_t type;              // Concept type
    void* semantics;                  // Semantic representation (e.g., embedding)
    uint32_t arity;                   // Number of arguments (for relations/functions)
    char** type_signature;            // Type signature (for type checking)
} concept_t;

/**
 * @brief Compositional expression tree
 */
typedef struct compositional_expr {
    composition_op_t op;              // Composition operation
    concept_t* concept;               // Leaf concept (if atomic)

    struct compositional_expr* left;  // Left child
    struct compositional_expr* right; // Right child
    struct compositional_expr** args; // Arguments (for n-ary operations)
    uint32_t num_args;                // Number of arguments

    void* semantics;                  // Computed semantics
    char** type;                      // Type of expression
} compositional_expr_t;

/**
 * @brief Compositional system configuration
 */
typedef struct {
    uint32_t max_primitives;          // Maximum primitive concepts
    uint32_t max_composition_depth;   // Maximum nesting depth
    bool enable_type_checking;        // Type check compositions
    bool enable_semantic_cache;       // Cache semantic computations
    float similarity_threshold;       // Threshold for concept similarity
} compositional_config_t;

/**
 * @brief Compositional system
 */
typedef struct {
    brain_t brain;                    // Parent brain

    concept_t** primitives;           // Array of primitive concepts
    uint32_t num_primitives;          // Number of primitives
    uint32_t capacity_primitives;     // Capacity

    compositional_config_t config;    // Configuration

    // Caches
    void* semantic_cache;             // Cache for computed semantics
    void* type_cache;                 // Cache for type derivations
} compositional_system_t;

// ============================================================================
// Core Functions
// ============================================================================

/**
 * @brief Create compositional system
 *
 * @param brain Parent brain instance
 * @param config Configuration
 * @return compositional_system_t* Compositional system, NULL on failure
 */
compositional_system_t* compositional_system_create(
    brain_t brain,
    const compositional_config_t* config
);

/**
 * @brief Destroy compositional system
 *
 * @param cs Compositional system
 */
void compositional_system_destroy(compositional_system_t* cs);

// ============================================================================
// Primitive Management
// ============================================================================

/**
 * @brief Add primitive concept
 *
 * @param cs Compositional system
 * @param name Concept name
 * @param type Concept type
 * @param arity Arity (for relations/functions)
 * @return uint32_t Concept ID, UINT32_MAX on failure
 */
uint32_t compositional_add_primitive(
    compositional_system_t* cs,
    const char* name,
    concept_type_t type,
    uint32_t arity
);

/**
 * @brief Get primitive concept by name
 *
 * @param cs Compositional system
 * @param name Concept name
 * @return concept_t* Concept, NULL if not found
 */
concept_t* compositional_get_primitive(
    compositional_system_t* cs,
    const char* name
);

/**
 * @brief Set concept semantics (embedding or logical form)
 *
 * @param cs Compositional system
 * @param name Concept name
 * @param semantics Semantic representation
 * @param semantics_size Size of semantics
 * @return bool True on success
 */
bool compositional_set_semantics(
    compositional_system_t* cs,
    const char* name,
    void* semantics,
    size_t semantics_size
);

// ============================================================================
// Composition Operations
// ============================================================================

/**
 * @brief Compose two expressions
 *
 * Creates a new compositional expression by combining two expressions
 * using the specified operation.
 *
 * @param cs Compositional system
 * @param op Composition operation
 * @param left Left operand
 * @param right Right operand
 * @return compositional_expr_t* Composed expression, NULL on failure
 *
 * @complexity O(d) where d = depth of expression tree
 */
compositional_expr_t* compositional_compose(
    compositional_system_t* cs,
    composition_op_t op,
    compositional_expr_t* left,
    compositional_expr_t* right
);

/**
 * @brief Apply function to arguments
 *
 * F(x, y, ...) where F is a function/relation and x, y are arguments
 *
 * @param cs Compositional system
 * @param function Function concept
 * @param args Array of argument expressions
 * @param num_args Number of arguments
 * @return compositional_expr_t* Result expression, NULL on failure
 */
compositional_expr_t* compositional_apply(
    compositional_system_t* cs,
    concept_t* function,
    compositional_expr_t** args,
    uint32_t num_args
);

/**
 * @brief Modify concept with modifier
 *
 * MODIFIER CONCEPT (e.g., "very fast", "red car")
 *
 * @param cs Compositional system
 * @param modifier Modifier concept
 * @param concept Concept to modify
 * @return compositional_expr_t* Modified expression
 */
compositional_expr_t* compositional_modify(
    compositional_system_t* cs,
    concept_t* modifier,
    compositional_expr_t* concept
);

/**
 * @brief Create conjunction
 *
 * A AND B
 *
 * @param cs Compositional system
 * @param expr1 First expression
 * @param expr2 Second expression
 * @return compositional_expr_t* Conjunction
 */
compositional_expr_t* compositional_and(
    compositional_system_t* cs,
    compositional_expr_t* expr1,
    compositional_expr_t* expr2
);

/**
 * @brief Create disjunction
 *
 * A OR B
 *
 * @param cs Compositional system
 * @param expr1 First expression
 * @param expr2 Second expression
 * @return compositional_expr_t* Disjunction
 */
compositional_expr_t* compositional_or(
    compositional_system_t* cs,
    compositional_expr_t* expr1,
    compositional_expr_t* expr2
);

/**
 * @brief Create negation
 *
 * NOT A
 *
 * @param cs Compositional system
 * @param expr Expression to negate
 * @return compositional_expr_t* Negation
 */
compositional_expr_t* compositional_not(
    compositional_system_t* cs,
    compositional_expr_t* expr
);

// ============================================================================
// Semantic Computation
// ============================================================================

/**
 * @brief Compute semantics of compositional expression
 *
 * Recursively computes the semantics of the expression tree using
 * composition rules (e.g., Montague semantics).
 *
 * @param cs Compositional system
 * @param expr Expression
 * @param semantics Output: semantic representation (caller allocates)
 * @param semantics_size Size of semantics buffer
 * @return bool True on success
 *
 * @complexity O(n) where n = number of nodes in expression tree
 */
bool compositional_compute_semantics(
    compositional_system_t* cs,
    compositional_expr_t* expr,
    void* semantics,
    size_t semantics_size
);

/**
 * @brief Evaluate expression in context
 *
 * Evaluates the expression given a context (e.g., state, knowledge base)
 * and returns the truth value or result.
 *
 * @param cs Compositional system
 * @param expr Expression to evaluate
 * @param context Evaluation context
 * @param result Output: evaluation result
 * @return bool True on success
 */
bool compositional_evaluate(
    compositional_system_t* cs,
    compositional_expr_t* expr,
    void* context,
    void* result
);

// ============================================================================
// Generalization
// ============================================================================

/**
 * @brief Learn compositional rule from examples
 *
 * Induces a compositional rule by finding systematic patterns in
 * input-output examples.
 *
 * Example:
 * Input: "jump", Output: "JUMP"
 * Input: "walk", Output: "WALK"
 * Learn: Command(X) = UPPERCASE(X)
 *
 * @param cs Compositional system
 * @param inputs Array of input expressions
 * @param outputs Array of output expressions
 * @param num_examples Number of examples
 * @param rule Output: learned compositional rule
 * @return bool True if rule learned
 */
bool compositional_learn_rule(
    compositional_system_t* cs,
    compositional_expr_t** inputs,
    compositional_expr_t** outputs,
    uint32_t num_examples,
    compositional_expr_t** rule
);

/**
 * @brief Apply compositional rule to novel input
 *
 * @param cs Compositional system
 * @param rule Learned rule
 * @param input Novel input
 * @param output Output: result of applying rule
 * @return bool True on success
 */
bool compositional_apply_rule(
    compositional_system_t* cs,
    compositional_expr_t* rule,
    compositional_expr_t* input,
    compositional_expr_t** output
);

/**
 * @brief Test systematic generalization
 *
 * Tests whether the system can generalize systematically to novel
 * combinations (e.g., SCAN benchmark).
 *
 * @param cs Compositional system
 * @param test_inputs Array of test inputs
 * @param expected_outputs Array of expected outputs
 * @param num_tests Number of tests
 * @param accuracy Output: proportion correct
 * @return bool True on success
 */
bool compositional_test_generalization(
    compositional_system_t* cs,
    compositional_expr_t** test_inputs,
    compositional_expr_t** expected_outputs,
    uint32_t num_tests,
    float* accuracy
);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Parse expression from string
 *
 * Parses a compositional expression from string notation.
 * Example: "AND(red(car), fast(car))" → compositional expression tree
 *
 * @param cs Compositional system
 * @param expr_string Expression string
 * @param expr Output: parsed expression (caller frees)
 * @return bool True on success
 */
bool compositional_parse(
    compositional_system_t* cs,
    const char* expr_string,
    compositional_expr_t** expr
);

/**
 * @brief Convert expression to string
 *
 * @param expr Expression
 * @param buffer Output buffer (caller allocates)
 * @param buffer_size Size of buffer
 * @return bool True on success
 */
bool compositional_to_string(
    compositional_expr_t* expr,
    char* buffer,
    size_t buffer_size
);

/**
 * @brief Print expression tree
 *
 * @param expr Expression
 * @param indent Indentation level
 */
void compositional_print_tree(
    compositional_expr_t* expr,
    uint32_t indent
);

/**
 * @brief Destroy expression
 *
 * @param expr Expression to destroy
 */
void compositional_expr_destroy(compositional_expr_t* expr);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_COMPOSITIONAL_H
```

#### 8.2.2 Implementation Notes

**Compositional Semantics (Montague Grammar)**

```
Semantic composition follows function-argument structure:

1. Atomic concepts have basic semantics:
   ⟦dog⟧ = {x : dog(x)}
   ⟦red⟧ = λx.red(x)

2. Function application:
   ⟦red dog⟧ = ⟦red⟧(⟦dog⟧) = {x : dog(x) ∧ red(x)}

3. Conjunction:
   ⟦A and B⟧ = ⟦A⟧ ∩ ⟦B⟧

4. Predication:
   ⟦John loves Mary⟧ = loves(john, mary)

5. Quantification:
   ⟦every dog barks⟧ = ∀x(dog(x) → bark(x))
```

**SCAN Benchmark Example**

```
Training:
- "jump" → "JUMP"
- "run" → "RUN"
- "jump twice" → "JUMP JUMP"
- "run twice" → "RUN RUN"

Test (compositional split):
- "jump thrice" → ? (should output "JUMP JUMP JUMP")
- "run thrice" → ? (should output "RUN RUN RUN")

Compositional rule learned:
- Command(X) = UPPERCASE(X)
- Modifier(X, "twice") = X X
- Modifier(X, "thrice") = X X X
- Parse("X twice") = Modifier(Command(X), "twice")
```

---

### 8.3 Analogical Reasoning Module

#### 8.3.1 Header: `include/core/reasoning/nimcp_analogical_reasoning.h`

```c
/**
 * @file nimcp_analogical_reasoning.h
 * @brief Analogical reasoning and cross-domain transfer
 *
 * Implements structure mapping engine (SME) for analogical transfer.
 * Enables reasoning by analogy: A:B::C:? and cross-domain knowledge transfer.
 *
 * Capabilities:
 * - Structure mapping between domains
 * - Analogical inference
 * - Cross-domain transfer
 *
 * References:
 * - Gentner (1983). Structure-mapping: A theoretical framework
 * - Falkenhainer, Forbus, Gentner (1989). Structure-mapping engine
 */

#ifndef NIMCP_ANALOGICAL_REASONING_H
#define NIMCP_ANALOGICAL_REASONING_H

#include "core/brain/nimcp_brain.h"
#include "core/reasoning/nimcp_compositional.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Type Definitions
// ============================================================================

/**
 * @brief Domain representation
 */
typedef struct {
    char name[128];                   // Domain name

    // Entities in domain
    char** entities;                  // Entity names
    uint32_t num_entities;            // Number of entities

    // Relations in domain
    char** relations;                 // Relation names
    uint32_t num_relations;           // Number of relations

    // Relational structure
    void* structure;                  // Graph representation of relations
} domain_t;

/**
 * @brief Mapping correspondence
 */
typedef struct {
    char source_entity[128];          // Source domain entity
    char target_entity[128];          // Target domain entity
    float confidence;                 // Confidence in mapping (0-1)
} correspondence_t;

/**
 * @brief Analogy structure
 */
typedef struct {
    domain_t* source;                 // Source domain
    domain_t* target;                 // Target domain

    correspondence_t** correspondences; // Entity/relation mappings
    uint32_t num_correspondences;     // Number of mappings

    float structural_similarity;      // Overall similarity score
    float systematicity_score;        // Systematicity (interconnectedness)
} analogy_t;

/**
 * @brief Analogical inference result
 */
typedef struct {
    char source_fact[256];            // Source domain fact
    char inferred_fact[256];          // Inferred target fact
    float confidence;                 // Confidence in inference
    char** support;                   // Supporting correspondences
    uint32_t num_support;             // Number of supporting mappings
} analogical_inference_t;

/**
 * @brief Analogical engine configuration
 */
typedef struct {
    float min_similarity_threshold;   // Minimum similarity to consider
    float systematicity_weight;       // Weight for systematicity
    float semantic_similarity_weight; // Weight for semantic similarity
    uint32_t max_correspondences;     // Max correspondences to search
    bool prefer_higher_order;         // Prefer higher-order relations
} analogical_config_t;

/**
 * @brief Analogical reasoning engine
 */
typedef struct {
    brain_t brain;                    // Parent brain
    analogical_config_t config;       // Configuration

    // Analogy cache
    analogy_t** cached_analogies;     // Previously found analogies
    uint32_t num_cached;              // Number cached
} analogical_engine_t;

// ============================================================================
// Core Functions
// ============================================================================

/**
 * @brief Create analogical reasoning engine
 *
 * @param brain Parent brain instance
 * @param config Configuration
 * @return analogical_engine_t* Analogical engine, NULL on failure
 */
analogical_engine_t* analogical_engine_create(
    brain_t brain,
    const analogical_config_t* config
);

/**
 * @brief Destroy analogical engine
 *
 * @param engine Analogical engine
 */
void analogical_engine_destroy(analogical_engine_t* engine);

// ============================================================================
// Domain Management
// ============================================================================

/**
 * @brief Create domain representation
 *
 * @param name Domain name
 * @param entities Array of entity names
 * @param num_entities Number of entities
 * @param relations Array of relation descriptions
 * @param num_relations Number of relations
 * @return domain_t* Domain, NULL on failure
 */
domain_t* analogical_create_domain(
    const char* name,
    const char** entities,
    uint32_t num_entities,
    const char** relations,
    uint32_t num_relations
);

/**
 * @brief Destroy domain
 *
 * @param domain Domain to destroy
 */
void analogical_destroy_domain(domain_t* domain);

/**
 * @brief Add relation to domain
 *
 * @param domain Domain
 * @param relation Relation description (e.g., "orbits(earth, sun)")
 * @return bool True on success
 */
bool analogical_add_relation(
    domain_t* domain,
    const char* relation
);

// ============================================================================
// Structure Mapping
// ============================================================================

/**
 * @brief Find analogy between domains using structure mapping
 *
 * Implements structure-mapping engine (SME) algorithm:
 * 1. Match: Find local matches between elements
 * 2. Induce: Build global interpretations (mappings)
 * 3. Evaluate: Score interpretations by structural consistency
 *
 * @param engine Analogical engine
 * @param source Source domain
 * @param target Target domain
 * @return analogy_t* Best analogy, NULL if none found
 *
 * @complexity O(n^2 * m) where n=entities, m=relations
 */
analogy_t* analogical_find_mapping(
    analogical_engine_t* engine,
    domain_t* source,
    domain_t* target
);

/**
 * @brief Compute structural similarity between domains
 *
 * Based on:
 * - Relational overlap
 * - Systematicity (interconnected relations)
 * - Semantic similarity of mapped elements
 *
 * @param engine Analogical engine
 * @param source Source domain
 * @param target Target domain
 * @param similarity Output: similarity score (0-1)
 * @return bool True on success
 */
bool analogical_compute_similarity(
    analogical_engine_t* engine,
    domain_t* source,
    domain_t* target,
    float* similarity
);

/**
 * @brief Get mapping for entity
 *
 * @param analogy Analogy structure
 * @param source_entity Source entity name
 * @param target_entity Output: corresponding target entity
 * @param max_len Maximum length of target_entity buffer
 * @return bool True if mapping found
 */
bool analogical_get_mapping(
    analogy_t* analogy,
    const char* source_entity,
    char* target_entity,
    size_t max_len
);

// ============================================================================
// Analogical Inference
// ============================================================================

/**
 * @brief Transfer knowledge from source to target domain
 *
 * Given an established analogy, transfers a fact from source to target.
 *
 * Example:
 * Source: solar_system, "revolves(earth, sun)"
 * Target: atom
 * Analogy: sun↔nucleus, earth↔electron
 * Transfer: "revolves(electron, nucleus)"
 *
 * @param engine Analogical engine
 * @param analogy Established analogy
 * @param source_fact Fact in source domain
 * @param inference Output: inferred fact in target (caller allocates)
 * @return bool True if inference successful
 */
bool analogical_transfer_knowledge(
    analogical_engine_t* engine,
    analogy_t* analogy,
    const char* source_fact,
    analogical_inference_t* inference
);

/**
 * @brief Solve analogy problem A:B::C:?
 *
 * @param engine Analogical engine
 * @param a First element
 * @param b Second element
 * @param c Third element
 * @param d Output: fourth element (caller allocates buffer)
 * @param max_len Maximum length of d buffer
 * @return bool True if analogy solved
 */
bool analogical_solve_problem(
    analogical_engine_t* engine,
    const char* a,
    const char* b,
    const char* c,
    char* d,
    size_t max_len
);

/**
 * @brief Generate candidate inferences from analogy
 *
 * @param engine Analogical engine
 * @param analogy Established analogy
 * @param inferences Output: array of candidate inferences (caller frees)
 * @param num_inferences Output: number of inferences
 * @return bool True on success
 */
bool analogical_generate_inferences(
    analogical_engine_t* engine,
    analogy_t* analogy,
    analogical_inference_t*** inferences,
    uint32_t* num_inferences
);

// ============================================================================
// Systematicity & Evaluation
// ============================================================================

/**
 * @brief Compute systematicity score
 *
 * Systematicity = degree of interconnectedness in mapped relations
 * Higher systematicity = more coherent mapping
 *
 * @param analogy Analogy
 * @return float Systematicity score (0-1)
 */
float analogical_compute_systematicity(analogy_t* analogy);

/**
 * @brief Evaluate analogy quality
 *
 * @param analogy Analogy
 * @param quality Output: quality metrics
 * @return bool True on success
 */
typedef struct {
    float structural_consistency;     // Internal consistency of mapping
    float systematicity;              // Interconnectedness
    float semantic_similarity;        // Semantic overlap
    float pragmatic_centrality;       // Relevance to goal
    float overall_quality;            // Weighted combination
} analogy_quality_t;

bool analogical_evaluate_quality(
    analogy_t* analogy,
    analogy_quality_t* quality
);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Print analogy mapping
 *
 * @param analogy Analogy
 */
void analogical_print_mapping(analogy_t* analogy);

/**
 * @brief Export analogy to GraphViz DOT format
 *
 * @param analogy Analogy
 * @param filename Output file
 * @return bool True on success
 */
bool analogical_export_dot(
    analogy_t* analogy,
    const char* filename
);

/**
 * @brief Destroy analogy
 *
 * @param analogy Analogy to destroy
 */
void analogical_destroy_analogy(analogy_t* analogy);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_ANALOGICAL_REASONING_H
```

#### 8.3.2 Implementation Notes

**Structure Mapping Engine (SME) Algorithm**

```
Phase 1: MATCH
  For each entity in source:
    For each entity in target:
      Compute local similarity (semantic, featural)
      Create candidate correspondence if similarity > threshold

Phase 2: INDUCE
  For each candidate correspondence:
    Build maximal structurally consistent interpretation
    Check parallel connectivity (if A↔A' and B↔B', require rel(A,B)↔rel(A',B'))

Phase 3: EVALUATE
  For each interpretation:
    Compute systematicity (interconnectedness score)
    Compute structural evaluation (global consistency)
    Rank interpretations by (systematicity + semantic similarity)

Return: Best scoring interpretation

Complexity: O(n^k) where n=entities, k=max_relation_arity
Optimized: O(n^2 * m) with heuristics
```

**Example: Solar System → Atom Analogy**

```
Source (Solar System):
  Entities: {sun, earth, mars}
  Relations: {
    "attracts(sun, earth)",
    "attracts(sun, mars)",
    "revolves(earth, sun)",
    "revolves(mars, sun)",
    "massive(sun)",
    "hotter(sun, earth)"
  }

Target (Atom):
  Entities: {nucleus, electron1, electron2}
  Relations: {
    "attracts(nucleus, electron1)",
    "attracts(nucleus, electron2)",
    "massive(nucleus)"
  }

SME Mapping:
  sun ↔ nucleus        (both massive, both attractors)
  earth ↔ electron1    (both attracted, both revolvers)
  mars ↔ electron2     (both attracted, both revolvers)

  attracts ↔ attracts
  revolves ↔ ? (INFER: electrons revolve around nucleus)

Systematicity: HIGH (interconnected relations all map)
Inference: "revolves(electron1, nucleus)", "revolves(electron2, nucleus)"
```

---

*[Continued in next section due to length...]*

Would you like me to continue with:
- Meta-Learning Module (8.4)
- World Model Module (8.5)
- Concept Formation Module (8.6)
- Complete API specifications (Section 9)
- Data structures (Section 10)
- Algorithms (Section 11)
- Testing strategy (Section 12)
- Performance requirements (Section 13)
- Migration path (Section 14)

This document is already quite extensive. I can continue adding the remaining modules and sections. Should I proceed?
