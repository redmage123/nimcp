# Imagination Engine Integration Map

**Version**: 1.0.0
**Date**: 2026-01-02
**Status**: Implementation In Progress

## Overview

The Imagination Engine enables generative mental simulation - the ability to construct, manipulate, and explore hypothetical scenarios, visual scenes, counterfactual possibilities, and prospective simulations.

## System Integration Map

```
╔═══════════════════════════════════════════════════════════════════════════════════════════════════════════╗
║                           NIMCP IMAGINATION ENGINE - SYSTEM INTEGRATION MAP                                ║
╠═══════════════════════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                                            ║
║  ┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐  ║
║  │                                        BRAIN FACTORY                                                 │  ║
║  │                          nimcp_brain_factory.h / nimcp_brain_init_cognitive.h                       │  ║
║  │                                                                                                      │  ║
║  │   imagination_engine_init_for_brain() ──► Creates & attaches engine to brain_t                     │  ║
║  │   brain_get_imagination_engine()      ──► Retrieves engine from brain                               │  ║
║  └──────────────────────────────────────────────────┬──────────────────────────────────────────────────┘  ║
║                                                     │                                                      ║
║                                                     ▼                                                      ║
║  ╔═════════════════════════════════════════════════════════════════════════════════════════════════════╗  ║
║  ║                                    IMAGINATION ENGINE                                                ║  ║
║  ║                              nimcp_imagination_engine.h                                              ║  ║
║  ║  ┌─────────────────────────────────────────────────────────────────────────────────────────────────┐║  ║
║  ║  │                                                                                                  │║  ║
║  ║  │  ┌────────────────┐   ┌────────────────┐   ┌────────────────┐   ┌────────────────────────────┐ │║  ║
║  ║  │  │   CONTROLLER   │   │   GENERATOR    │   │   EVALUATOR    │   │        WORKSPACE           │ │║  ║
║  ║  │  │                │   │                │   │                │   │                            │ │║  ║
║  ║  │  │ • Goal→Latent  │   │ • Latent→Visual│   │ • Coherence    │   │ • Active Scenarios         │ │║  ║
║  ║  │  │ • Top-down     │   │ • Latent→Audio │   │ • Plausibility │   │ • Trajectory History       │ │║  ║
║  ║  │  │   guidance     │   │ • Noise inject │   │ • Reality dist │   │ • Element Buffer           │ │║  ║
║  ║  │  └───────┬────────┘   └───────┬────────┘   └───────┬────────┘   └────────────────────────────┘ │║  ║
║  ║  │          │                    │                    │                                            │║  ║
║  ║  │          └────────────────────┴────────────────────┘                                            │║  ║
║  ║  └─────────────────────────────────────────────────────────────────────────────────────────────────┘║  ║
║  ╚══════════════════════════════════════════╤══════════════════════════════════════════════════════════╝  ║
║                                              │                                                             ║
║  ════════════════════════════════════════════╪═════════════════════════════════════════════════════════   ║
║                                              │                                                             ║
║              ┌───────────────────────────────┼───────────────────────────────┐                             ║
║              │                               │                               │                             ║
║              ▼                               ▼                               ▼                             ║
║  ╔═══════════════════════╗    ╔═══════════════════════╗    ╔═══════════════════════╗                      ║
║  ║   COGNITIVE LAYER     ║    ║   PERCEPTION LAYER    ║    ║   MEMORY LAYER        ║                      ║
║  ╠═══════════════════════╣    ╠═══════════════════════╣    ╠═══════════════════════╣                      ║
║  ║                       ║    ║                       ║    ║                       ║                      ║
║  ║ ┌───────────────────┐ ║    ║ ┌───────────────────┐ ║    ║ ┌───────────────────┐ ║                      ║
║  ║ │ PREFRONTAL CORTEX │ ║    ║ │  VISUAL CORTEX    │ ║    ║ │   HIPPOCAMPUS     │ ║                      ║
║  ║ │                   │ ║    ║ │                   │ ║    ║ │                   │ ║                      ║
║  ║ │ ► Executive ctrl  │ ║    ║ │ ► Visual generate │ ║    ║ │ ► Pattern complete│ ║                      ║
║  ║ │ ► Goal setting    │ ║    ║ │ ► Feedback loop   │ ║    ║ │ ► Scene construct │ ║                      ║
║  ║ │ ► Inhibition      │ ║    ║ │ ► Mental imagery  │ ║    ║ │ ► Memory seeds    │ ║                      ║
║  ║ └─────────┬─────────┘ ║    ║ └─────────┬─────────┘ ║    ║ └─────────┬─────────┘ ║                      ║
║  ║           │           ║    ║           │           ║    ║           │           ║                      ║
║  ║ ┌─────────▼─────────┐ ║    ║ ┌─────────▼─────────┐ ║    ║ ┌─────────▼─────────┐ ║                      ║
║  ║ │ GLOBAL WORKSPACE  │ ║    ║ │  AUDIO CORTEX     │ ║    ║ │ ENGRAM STORE      │ ║                      ║
║  ║ │                   │ ║    ║ │                   │ ║    ║ │                   │ ║                      ║
║  ║ │ ► Conscious access│ ║    ║ │ ► Audio generate  │ ║    ║ │ ► Memory encoding │ ║                      ║
║  ║ │ ► Broadcast imgn  │ ║    ║ │ ► Inner speech    │ ║    ║ │ ► Reconsolidation │ ║                      ║
║  ║ │ ► Competition     │ ║    ║ │ ► Sound imagery   │ ║    ║ │ ► Retrieval cues  │ ║                      ║
║  ║ └─────────┬─────────┘ ║    ║ └─────────┬─────────┘ ║    ║ └─────────┬─────────┘ ║                      ║
║  ║           │           ║    ║           │           ║    ║           │           ║                      ║
║  ║ ┌─────────▼─────────┐ ║    ║ ┌─────────▼─────────┐ ║    ║ ┌─────────▼─────────┐ ║                      ║
║  ║ │ THEORY OF MIND    │ ║    ║ │  SPEECH CORTEX    │ ║    ║ │ SEMANTIC MEMORY   │ ║                      ║
║  ║ │                   │ ║    ║ │                   │ ║    ║ │                   │ ║                      ║
║  ║ │ ► Agent simulate  │ ║    ║ │ ► Language gen    │ ║    ║ │ ► Concept lookup  │ ║                      ║
║  ║ │ ► Belief modeling │ ║    ║ │ ► Narrative       │ ║    ║ │ ► Knowledge base  │ ║                      ║
║  ║ │ ► Social scenes   │ ║    ║ │ ► Inner monologue │ ║    ║ │ ► Fact constraints│ ║                      ║
║  ║ └───────────────────┘ ║    ║ └───────────────────┘ ║    ║ └───────────────────┘ ║                      ║
║  ║                       ║    ║                       ║    ║                       ║                      ║
║  ║ ┌───────────────────┐ ║    ╚═══════════════════════╝    ╚═══════════════════════╝                      ║
║  ║ │    CURIOSITY      │ ║                                                                                ║
║  ║ │                   │ ║                                                                                ║
║  ║ │ ► Novelty drive   │ ║                                                                                ║
║  ║ │ ► Exploration     │ ║                                                                                ║
║  ║ │ ► Hypothesis gen  │ ║                                                                                ║
║  ║ └───────────────────┘ ║                                                                                ║
║  ╚═══════════════════════╝                                                                                ║
║                                                                                                            ║
║  ════════════════════════════════════════════════════════════════════════════════════════════════════════  ║
║                                                                                                            ║
║  ╔═══════════════════════╗    ╔═══════════════════════╗    ╔═══════════════════════╗                      ║
║  ║   WORLD MODEL LAYER   ║    ║   HOMEOSTATIC LAYER   ║    ║   LEARNING LAYER      ║                      ║
║  ╠═══════════════════════╣    ╠═══════════════════════╣    ╠═══════════════════════╣                      ║
║  ║                       ║    ║                       ║    ║                       ║                      ║
║  ║ ┌───────────────────┐ ║    ║ ┌───────────────────┐ ║    ║ ┌───────────────────┐ ║                      ║
║  ║ │ JEPA PREDICTOR    │ ║    ║ │ SLEEP/WAKE CYCLE  │ ║    ║ │ TRAINING MODULE   │ ║                      ║
║  ║ │                   │ ║    ║ │                   │ ║    ║ │                   │ ║                      ║
║  ║ │ ► Latent predict  │ ║    ║ │ ► REM creativity  │ ║    ║ │ ► Learn from imgn │ ║                      ║
║  ║ │ ► World dynamics  │ ║    ║ │ ► Dream replay    │ ║    ║ │ ► Gradient flow   │ ║                      ║
║  ║ │ ► State evolution │ ║    ║ │ ► Consolidation   │ ║    ║ │ ► Experience sim  │ ║                      ║
║  ║ └─────────┬─────────┘ ║    ║ └─────────┬─────────┘ ║    ║ └─────────┬─────────┘ ║                      ║
║  ║           │           ║    ║           │           ║    ║           │           ║                      ║
║  ║ ┌─────────▼─────────┐ ║    ║ ┌─────────▼─────────┐ ║    ║ ┌─────────▼─────────┐ ║                      ║
║  ║ │ FREE ENERGY/FEP   │ ║    ║ │ IMMUNE SYSTEM     │ ║    ║ │ PLASTICITY        │ ║                      ║
║  ║ │                   │ ║    ║ │                   │ ║    ║ │                   │ ║                      ║
║  ║ │ ► Prediction err  │ ║    ║ │ ► Inflammation    │ ║    ║ │ ► STDP modulation │ ║                      ║
║  ║ │ ► Surprise signal │ ║    ║ │   ↓ vividness    │ ║    ║ │ ► Hebbian update  │ ║                      ║
║  ║ │ ► Belief update   │ ║    ║ │   ↓ coherence    │ ║    ║ │ ► Synaptic change │ ║                      ║
║  ║ └───────────────────┘ ║    ║ │   ↓ capacity     │ ║    ║ └───────────────────┘ ║                      ║
║  ║                       ║    ║ └─────────┬─────────┘ ║    ╚═══════════════════════╝                      ║
║  ║ ┌───────────────────┐ ║    ║           │           ║                                                   ║
║  ║ │ NEURAL LOGIC      │ ║    ║ ┌─────────▼─────────┐ ║                                                   ║
║  ║ │                   │ ║    ║ │ NEURAL SUBSTRATE  │ ║                                                   ║
║  ║ │ ► Scene validity  │ ║    ║ │                   │ ║                                                   ║
║  ║ │ ► Logic gates     │ ║    ║ │ ► Metabolic limit │ ║                                                   ║
║  ║ │ ► Consistency     │ ║    ║ │ ► ATP/energy      │ ║                                                   ║
║  ║ └───────────────────┘ ║    ║ │ ► Capacity ctrl   │ ║                                                   ║
║  ╚═══════════════════════╝    ║ └───────────────────┘ ║                                                   ║
║                               ╚═══════════════════════╝                                                   ║
║                                                                                                            ║
║  ════════════════════════════════════════════════════════════════════════════════════════════════════════  ║
║                                                                                                            ║
║  ╔═══════════════════════╗    ╔═══════════════════════╗    ╔═══════════════════════╗                      ║
║  ║  INFRASTRUCTURE       ║    ║   ROUTING LAYER       ║    ║   ACCELERATION        ║                      ║
║  ╠═══════════════════════╣    ╠═══════════════════════╣    ╠═══════════════════════╣                      ║
║  ║                       ║    ║                       ║    ║                       ║                      ║
║  ║ ┌───────────────────┐ ║    ║ ┌───────────────────┐ ║    ║ ┌───────────────────┐ ║                      ║
║  ║ │ BIO-ASYNC ROUTER  │ ║    ║ │ THALAMIC ROUTER   │ ║    ║ │ GPU CONTEXT       │ ║                      ║
║  ║ │                   │ ║    ║ │                   │ ║    ║ │                   │ ║                      ║
║  ║ │ ► Async messaging │ ║    ║ │ ► Attention gate  │ ║    ║ │ ► CUDA kernels    │ ║                      ║
║  ║ │ ► Module coord    │ ║    ║ │ ► Priority route  │ ║    ║ │ ► Batch generate  │ ║                      ║
║  ║ │ ► Event dispatch  │ ║    ║ │ ► Signal routing  │ ║    ║ │ ► Parallel decode │ ║                      ║
║  ║ └───────────────────┘ ║    ║ └───────────────────┘ ║    ║ └───────────────────┘ ║                      ║
║  ╚═══════════════════════╝    ╚═══════════════════════╝    ╚═══════════════════════╝                      ║
║                                                                                                            ║
╚═══════════════════════════════════════════════════════════════════════════════════════════════════════════╝
```

## Integration Points Reference

### Bidirectional Data Flow Table

Each integration point has explicit INPUT (to imagination) and OUTPUT (from imagination) contracts:

| Component | INPUT → Imagination | OUTPUT ← Imagination | Integration Function |
|-----------|---------------------|----------------------|---------------------|
| **Prefrontal Cortex** | Goals, inhibition signals, executive commands, attention focus | Scenario status, goal completion, resource requests | `imagination_engine_connect_prefrontal()` |
| **Global Workspace** | Conscious goals, competing content, broadcast triggers | Imagined content for broadcast, salience scores | `imagination_engine_connect_global_workspace()` |
| **JEPA Predictor** | World model queries, state evolution requests | Scene states for prediction, latent trajectories | `imagination_engine_connect_world_model()` |
| **Hippocampus** | Memory seeds, retrieval cues, pattern completion requests | Pseudo-memories for consolidation, scene reconstructions | `imagination_engine_connect_hippocampus()` |
| **Visual Cortex** | Perceptual templates, bottom-up features, visual priors | Top-down generation requests, mental imagery buffers | `imagination_engine_connect_visual()` |
| **Audio Cortex** | Auditory templates, sound priors, speech patterns | Audio generation requests, inner speech/sound buffers | `imagination_engine_connect_audio()` |
| **Theory of Mind** | Agent beliefs, mental state models, social context | Social simulation requests, perspective-taking queries | `imagination_engine_connect_tom()` |
| **Curiosity** | Novelty signals, exploration triggers, information gain | Hypothesis scenarios, exploration outcomes | `imagination_engine_connect_curiosity()` |
| **Sleep Cycle** | REM creativity noise, dream triggers, consolidation signals | Dream content, replay requests | `imagination_engine_connect_sleep()` |
| **Immune System** | Inflammation level, cytokine signals, sickness behavior | Capacity requests (modulated by inflammation) | `imagination_engine_connect_immune()` |
| **Neural Substrate** | ATP levels, glucose, temperature, metabolic limits | Energy consumption reports, resource demands | `imagination_engine_connect_substrate()` |
| **Thalamic Router** | Attention gates, priority signals, routing tables | Content for routing, attention requests | `imagination_engine_connect_thalamic()` |
| **Training Module** | Learning signals, gradient requests, experience replay | Imagined experiences for training, prediction errors | `imagination_engine_connect_training()` |
| **Neural Logic** | Logical constraints, validity rules, consistency checks | Scene states for validation, constraint queries | `imagination_engine_connect_logic()` |
| **Parietal Lobe** | Spatial transforms, numerical intuition, domain knowledge | Spatial manipulation requests, math/science queries | `imagination_engine_connect_parietal()` |
| **Quantum Reasoning** | Constraint satisfaction requests, superposition queries | Quantum search results, collapsed solutions | `imagination_engine_connect_quantum_kb()` |
| **GPU Context** | Batch generation requests, kernel dispatch | Generated content, computation results | `imagination_engine_init_gpu()` |
| **Bio-Async Router** | Module events, async notifications, phase signals | Broadcast messages, coordination requests | `imagination_engine_connect_bio_async()` |

### Parietal Lobe Domain Submodules

| Submodule | INPUT → Imagination | OUTPUT ← Imagination |
|-----------|---------------------|----------------------|
| **Spatial Reasoning** | Mental rotation matrices, viewpoint transforms | Spatial query results, transformed scenes |
| **Number Sense** | Magnitude comparisons, quantity estimates | Numerical visualization requests |
| **Mathematical Intuition** | Pattern features, symmetry detection | Math concept queries, equation visualization |
| **Scientific Reasoning** | Hypothesis structures, causal models | Simulation requests, prediction queries |
| **Chemistry** | Molecular structures, reaction conditions | Chemistry simulation scenarios |
| **Biology** | Biological systems, perturbation models | Biology simulation scenarios |
| **Physics** | Physical states, force vectors | Physics simulation scenarios |
| **Software Engineering** | Program states, algorithm traces | Code visualization, execution traces |
| **Equation Manipulation** | Symbolic expressions, variable bindings | Equation solving, symbolic manipulation |

### Imagination Modes & Pathways

| Mode | Primary Pathway | Key Integrations |
|------|-----------------|------------------|
| DIRECTED | Prefrontal → Controller → Generator | Prefrontal, Global Workspace |
| COUNTERFACTUAL | Hippocampus → JEPA → Evaluator | Hippocampus, JEPA, Logic |
| PROSPECTIVE | JEPA → Controller → Generator → Evaluator | JEPA, Prefrontal, Training |
| RETROSPECTIVE | Hippocampus → Generator → Visual Cortex | Hippocampus, Engrams |
| CREATIVE | Sleep → Noise → Generator → Curiosity | Sleep, Curiosity, REM noise |
| SOCIAL | ToM → Controller → Generator → Evaluator | Theory of Mind, Prefrontal |
| PASSIVE | Default Mode → Generator (low control) | Global Workspace, Hippocampus |

### Modulation Effects

| Source | Effect on Imagination |
|--------|----------------------|
| Inflammation | ↓ Vividness, ↓ Coherence, ↓ Capacity |
| Low ATP/Glucose | ↓ Generation speed, ↓ Detail |
| High Attention | ↑ Directed control, ↓ Spontaneous drift |
| REM Sleep | ↑ Creativity noise, ↑ Novel combinations |
| Thalamic Gating | Route imagination to relevant cortical areas |

## File Structure

### Headers (include/cognitive/imagination/)

| File | Purpose |
|------|---------|
| `nimcp_imagination_engine.h` | Main engine API |
| `nimcp_imagination_workspace.h` | Scenario workspace |
| `nimcp_imagination_generator.h` | Scene generation |
| `nimcp_imagination_controller.h` | Top-down control |
| `nimcp_imagination_evaluator.h` | Coherence/reality check |
| `nimcp_imagination_memory_bridge.h` | Hippocampus bridge |
| `nimcp_imagination_immune_bridge.h` | Immune modulation |
| `nimcp_imagination_thalamic_bridge.h` | Thalamic routing |
| `nimcp_imagination_substrate_bridge.h` | Metabolic constraints |
| `nimcp_imagination_gpu.h` | GPU acceleration |

### Implementations (src/cognitive/imagination/)

| File | Purpose |
|------|---------|
| `nimcp_imagination_engine.c` | Engine implementation |
| `nimcp_imagination_generator.c` | Generator implementation |
| `nimcp_imagination_controller.c` | Controller implementation |
| `nimcp_imagination_evaluator.c` | Evaluator implementation |

### GPU Kernels (src/gpu/imagination/)

| File | Purpose |
|------|---------|
| `nimcp_imagination_kernels.cu` | CUDA generation kernels |

## Test Requirements

### Unit Tests (test/unit/)

Each component requires isolated unit tests:

1. **test_imagination_engine.cpp**
   - Engine creation/destruction
   - Configuration validation
   - Connection management
   - Scenario lifecycle

2. **test_imagination_generator.cpp**
   - Visual generation from latent
   - Audio generation from latent
   - Noise injection
   - Scene blending

3. **test_imagination_controller.cpp**
   - Goal parsing
   - Guidance computation
   - Control strength modulation

4. **test_imagination_evaluator.cpp**
   - Coherence calculation
   - Plausibility checking
   - Reality distance measurement

5. **test_imagination_workspace.cpp**
   - Scenario allocation/release
   - Buffer management
   - Trajectory tracking

### Integration Tests (test/integration/)

Test interactions between components:

1. **test_imagination_jepa_integration.cpp**
   - JEPA latent prediction with imagination
   - World model queries

2. **test_imagination_hippocampus_integration.cpp**
   - Memory-based scene construction
   - Pattern completion

3. **test_imagination_visual_integration.cpp**
   - Visual cortex generation feedback
   - Mental imagery pipeline

4. **test_imagination_immune_integration.cpp**
   - Inflammation modulation effects
   - Capacity reduction under stress

5. **test_imagination_bio_async_integration.cpp**
   - Message passing
   - Event coordination

### Regression Tests (test/regression/)

Ensure stability:

1. **test_imagination_regression.cpp**
   - Vividness consistency
   - Coherence thresholds
   - Performance benchmarks

### E2E Tests (test/e2e/)

Full pipeline tests:

1. **test_imagination_e2e.cpp**
   - Full directed imagination scenario
   - Counterfactual reasoning pipeline
   - Prospective simulation pipeline
   - Social simulation with ToM
   - Creative recombination

2. **test_imagination_gpu_e2e.cpp**
   - GPU-accelerated generation
   - Batch processing

## API Quick Reference

```c
// Lifecycle
imagination_engine_t* imagination_engine_create(const imagination_engine_config_t* config);
void imagination_engine_destroy(imagination_engine_t* engine);

// Brain Factory
int imagination_engine_init_for_brain(brain_t brain, const imagination_engine_config_t* config);
imagination_engine_t* brain_get_imagination_engine(brain_t brain);

// Connections
int imagination_engine_connect_world_model(imagination_engine_t* engine, jepa_predictor_t* jepa);
int imagination_engine_connect_hippocampus(imagination_engine_t* engine, hippocampus_adapter_t* hipp);
int imagination_engine_connect_visual(imagination_engine_t* engine, visual_cortex_t* visual);
int imagination_engine_connect_immune(imagination_engine_t* engine, brain_immune_system_t* immune);
int imagination_engine_connect_thalamic(imagination_engine_t* engine, thalamic_router_t* thalamic);
int imagination_engine_init_gpu(imagination_engine_t* engine, int device_id);

// Scenario Management
imagination_scenario_t* imagination_begin_scenario(imagination_engine_t* engine, imagination_mode_t mode, const imagination_goal_t* goal);
int imagination_step_scenario(imagination_engine_t* engine, imagination_scenario_t* scenario);
int imagination_end_scenario(imagination_engine_t* engine, imagination_scenario_t* scenario);

// Generation
int imagination_generate_visual(imagination_engine_t* engine, imagination_scenario_t* scenario);
int imagination_generate_audio(imagination_engine_t* engine, imagination_scenario_t* scenario);
int imagination_generate_multimodal(imagination_engine_t* engine, imagination_scenario_t* scenario);

// Advanced Modes
imagination_scenario_t* imagination_counterfactual(imagination_engine_t* engine, const nimcp_tensor_t* memory, const counterfactual_query_t* query);
imagination_scenario_t* imagination_simulate_future(imagination_engine_t* engine, const nimcp_tensor_t* current_state, const nimcp_tensor_t* actions, size_t num_actions, size_t steps_ahead);
imagination_scenario_t* imagination_simulate_agent(imagination_engine_t* engine, uint64_t agent_id, const nimcp_tensor_t* believed_state);
imagination_scenario_t* imagination_creative_recombine(imagination_engine_t* engine, nimcp_tensor_t** seed_memories, size_t num_memories, float creativity_level);

// Evaluation
int imagination_evaluate(imagination_engine_t* engine, imagination_scenario_t* scenario, imagination_evaluation_t* result);
```

## Implementation Checklist

- [ ] Phase 1: Core Infrastructure
  - [ ] imagination_workspace.h (DONE)
  - [ ] imagination_engine.h (DONE)
  - [ ] imagination_engine.c

- [ ] Phase 2: Generation
  - [ ] imagination_generator.h
  - [ ] imagination_generator.c

- [ ] Phase 3: Control
  - [ ] imagination_controller.h
  - [ ] imagination_controller.c

- [ ] Phase 4: Evaluation
  - [ ] imagination_evaluator.h
  - [ ] imagination_evaluator.c

- [ ] Phase 5: Memory Integration
  - [ ] imagination_memory_bridge.h

- [ ] Phase 6: Integration Bridges
  - [ ] imagination_immune_bridge.h
  - [ ] imagination_thalamic_bridge.h
  - [ ] imagination_substrate_bridge.h

- [ ] Phase 7: GPU Acceleration
  - [ ] imagination_gpu.h
  - [ ] imagination_kernels.cu

- [ ] Phase 8: Tests
  - [ ] Unit tests
  - [ ] Integration tests
  - [ ] Regression tests
  - [ ] E2E tests

- [ ] Phase 9: CMake Integration
  - [ ] Update src/lib/CMakeLists.txt
  - [ ] Update test/unit/CMakeLists.txt
