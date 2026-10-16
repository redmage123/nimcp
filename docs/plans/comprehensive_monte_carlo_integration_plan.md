# Comprehensive Monte Carlo Integration Plan

## Overview

Integrate Monte Carlo Simulation (MCS) and Monte Carlo Tree Search (MCTS) across the NIMCP codebase for quantum algorithms and cognitive modules.

**Scope**: 45+ modules identified for integration
**New Module**: `nimcp_quantum_monte_carlo.h/c` - Quantum-specific MC utilities

---

## Phase 1: Quantum Monte Carlo Module

### New Files
- `include/utils/quantum/nimcp_quantum_monte_carlo.h`
- `src/utils/quantum/nimcp_quantum_monte_carlo.c`
- `test/unit/utils/quantum/test_quantum_monte_carlo.cpp`

### API Design
```c
// Quantum amplitude estimation via importance sampling
nimcp_mc_result_t qmc_estimate_amplitude(
    const float* amplitudes,
    uint32_t num_states,
    uint32_t target_state,
    uint32_t num_samples,
    mc_result_t* result
);

// Quantum measurement with importance sampling
uint32_t qmc_measure_importance(
    const float* amplitudes,
    uint32_t num_states,
    const float* proposal,
    uint32_t* seed
);

// Partition function estimation for annealing
nimcp_mc_result_t qmc_estimate_partition(
    energy_function_t energy_fn,
    float temperature,
    uint32_t dim,
    uint32_t num_samples,
    void* user_data,
    mc_result_t* result
);

// Finite-shot measurement simulation
nimcp_mc_result_t qmc_finite_shots(
    const float* amplitudes,
    uint32_t num_states,
    uint32_t num_shots,
    uint32_t* counts,
    float* uncertainties,
    uint32_t* seed
);

// Adaptive annealing with M-H
nimcp_mc_result_t qmc_adaptive_anneal(
    energy_function_t energy_fn,
    float* state,
    uint32_t dim,
    qmc_anneal_config_t* config,
    qmc_anneal_result_t* result
);

// Quantum walk with MCTS-guided coin selection
nimcp_mc_result_t qmc_walk_mcts(
    quantum_walker_t* walker,
    uint32_t target_node,
    uint32_t num_iterations,
    qmc_walk_result_t* result
);
```

---

## Phase 2: Quantum Algorithm Integrations

### 2.1 Quantum Walk (`src/utils/quantum/nimcp_quantum_walk.c`)

| Function | Integration | Method |
|----------|-------------|--------|
| `quantum_walk_measure()` | Importance sampling | MCS |
| `quantum_walk_step()` | Adaptive coin selection | MCTS |
| `quantum_walk_compute_stats()` | Stratified entropy estimation | MCS |

**Implementation**:
```c
// Replace line 577-592 with importance sampling
uint32_t quantum_walk_measure_mc(quantum_walker_t* walker, uint32_t* seed) {
    // Use importance sampling weighted by |amplitude|^2
}

// Add MCTS-guided coin selection
nimcp_mc_result_t quantum_walk_step_mcts(
    quantum_walker_t* walker,
    uint32_t target_node,
    mcts_result_t* result
);
```

### 2.2 Quantum Shannon (`src/utils/quantum/nimcp_quantum_shannon.c`)

| Function | Integration | Method |
|----------|-------------|--------|
| `compute_node_entropy()` | Adaptive importance sampling | MCS |
| `update_channel_capacities()` | M-H synapse sampling | MCS |
| `detect_bottlenecks()` | Early-stopping MCMC | MCS |
| `update_shannon_metrics()` | Control variates | MCS |

### 2.3 Quantum Annealing (`src/optimization/quantum_annealing/`)

| Function | Integration | Method |
|----------|-------------|--------|
| `quantum_anneal()` | Adaptive MCMC proposal | MCS |
| Acceptance criterion | Metropolis tuning | MCS |
| Temperature schedule | MCTS schedule selection | MCTS |

### 2.4 Quantum Reasoning (`src/cognitive/reasoning/`)

| Function | Integration | Method |
|----------|-------------|--------|
| `qreason_solve_sat()` | MCTS variable ordering | MCTS |
| `qreason_measure()` | Importance sampling | MCS |
| `qreason_forward_chain()` | MCTS rule ordering | MCTS |

---

## Phase 3: Cognitive Module Integrations

### 3.1 Game Theory (HIGH PRIORITY)

| Module | File | Integration | Method |
|--------|------|-------------|--------|
| GT Learning | `nimcp_gt_learning.c` | Opponent modeling | MCTS |
| Coalition | `nimcp_gt_coalition.c` | Coalition search | MCTS + MCS |
| Auction | `nimcp_auction.c` | Strategic bidding | MCTS |
| Spatial GT | `nimcp_gt_spatial.c` | Spatial strategy | MCTS |

### 3.2 Planning & Executive (HIGH PRIORITY)

| Module | File | Integration | Method |
|--------|------|-------------|--------|
| Executive | `nimcp_executive.c` | Goal decomposition | MCTS |
| RCOG Engine | `nimcp_rcog_engine.c` | Task decomposition | MCTS |
| Tool Router | `nimcp_rcog_tool_router.c` | Tool sequence | MCTS |

### 3.3 Prediction & Uncertainty (MEDIUM PRIORITY)

| Module | File | Integration | Method |
|--------|------|-------------|--------|
| JEPA Predictor | `nimcp_jepa_predictor.c` | Trajectory uncertainty | MCS |
| JEPA Multimodal | `nimcp_jepa_multimodal.c` | Cross-modal sampling | MCS |
| Predictive | `nimcp_predictive.c` | Ensemble prediction | MCS |

### 3.4 Curiosity & Exploration (MEDIUM PRIORITY)

| Module | File | Integration | Method |
|--------|------|-------------|--------|
| Curiosity Enhanced | `nimcp_curiosity_enhanced.c` | Uncertainty estimation | MCS |
| Base Curiosity | `nimcp_curiosity.c` | Empowerment | MCS |

### 3.5 Knowledge & Reasoning (MEDIUM PRIORITY)

| Module | File | Integration | Method |
|--------|------|-------------|--------|
| KG Reader | `nimcp_kg_reader.c` | Random walk sampling | MCS |
| Symbolic Logic | `nimcp_symbolic_logic.c` | Proof search | MCS |
| Backward Chaining | `nimcp_backward_chaining.c` | Goal-directed search | MCTS |

### 3.6 Social & Emotional (LOWER PRIORITY)

| Module | File | Integration | Method |
|--------|------|-------------|--------|
| Theory of Mind | `nimcp_theory_of_mind.c` | Social simulation | MCTS |
| Remorse/Regret | `nimcp_remorse_regret.c` | Counterfactual sampling | MCS |
| Wellbeing | `nimcp_wellbeing_prediction.c` | Life trajectory | MCS |

### 3.7 Memory & Workspace (LOWER PRIORITY)

| Module | File | Integration | Method |
|--------|------|-------------|--------|
| Global Workspace | `nimcp_global_workspace.c` | Arbitration | MCTS |
| GW Shannon | `nimcp_global_workspace_shannon.c` | MI estimation | MCS |
| Working Memory | `nimcp_working_memory.c` | Rehearsal scheduling | MCTS |

### 3.8 Meta-Learning & Adaptation (LOWER PRIORITY)

| Module | File | Integration | Method |
|--------|------|-------------|--------|
| Meta-Learning | `nimcp_meta_learning.c` | Task sampling | MCS |
| Brain Strategy | `nimcp_brain_strategy.c` | Strategy selection | MCTS |

---

## Phase 4: Test Suite

### Test Files to Create
1. `test/unit/utils/quantum/test_quantum_monte_carlo.cpp` - Core QMC tests
2. `test/unit/quantum/test_quantum_walk_mc.cpp` - Walk MC integration tests
3. `test/unit/quantum/test_quantum_shannon_mc.cpp` - Shannon MC tests
4. `test/unit/quantum/test_quantum_annealing_mc.cpp` - Annealing MC tests
5. `test/unit/cognitive/test_game_theory_mc.cpp` - Game theory MC tests
6. `test/unit/cognitive/test_executive_mcts.cpp` - Executive MCTS tests
7. `test/unit/cognitive/test_jepa_mc.cpp` - JEPA MC tests
8. `test/integration/test_mc_integration.cpp` - Full integration tests

### Test Categories
1. **Unit Tests**: Individual MC function correctness
2. **Convergence Tests**: MC estimates converge to known values
3. **Variance Reduction Tests**: Importance sampling reduces variance
4. **MCTS Tests**: Tree search finds optimal actions
5. **Integration Tests**: MC improves module performance

---

## Implementation Order

| Step | Task | Files | Status |
|------|------|-------|--------|
| 1 | Create QMC header | `nimcp_quantum_monte_carlo.h` | PENDING |
| 2 | Implement QMC core | `nimcp_quantum_monte_carlo.c` | PENDING |
| 3 | Quantum Walk MC | `nimcp_quantum_walk.c` | PENDING |
| 4 | Quantum Shannon MC | `nimcp_quantum_shannon.c` | PENDING |
| 5 | Quantum Annealing MC | `nimcp_quantum_annealing.c` | PENDING |
| 6 | Quantum Reasoning MC | `nimcp_quantum_reasoning.h` | PENDING |
| 7 | Game Theory MCTS | `nimcp_gt_learning.c` | PENDING |
| 8 | Executive MCTS | `nimcp_executive.c` | PENDING |
| 9 | JEPA MCS | `nimcp_jepa_predictor.c` | PENDING |
| 10 | Curiosity MCS | `nimcp_curiosity_enhanced.c` | PENDING |
| 11 | KG Random Walk | `nimcp_kg_reader.c` | PENDING |
| 12 | Theory of Mind MCTS | `nimcp_theory_of_mind.c` | PENDING |
| 13 | Write unit tests | `test_quantum_monte_carlo.cpp` | PENDING |
| 14 | Write integration tests | `test_mc_integration.cpp` | PENDING |
| 15 | Final build and verify | - | PENDING |

---

## Expected Benefits

| Module Category | Method | Expected Improvement |
|-----------------|--------|---------------------|
| Quantum Walk | Importance Sampling | 2-5x variance reduction |
| Quantum Shannon | Stratified Sampling | 3-10x faster bottleneck detection |
| Quantum Annealing | Adaptive MCMC | 5-10x faster convergence |
| Game Theory | MCTS | 10-20x better opponent modeling |
| Executive Planning | MCTS | 5-10x better goal decomposition |
| JEPA | MCS | Uncertainty quantification |
| Curiosity | MCS | Better exploration guidance |

---

## Dependencies

- `nimcp_monte_carlo.h` (already implemented)
- `nimcp_quantum_walk.h`
- `nimcp_quantum_shannon.h`
- `nimcp_quantum_annealing.h`
- `nimcp_quantum_reasoning.h`

---

## Notes

1. All integrations use the existing callback-based MC API from `nimcp_monte_carlo.h`
2. Thread-safe RNG via `mc_random_*()` functions
3. Preserve existing APIs - add new `*_mc()` or `*_mcts()` variants
4. Stack allocation for small trees (< 256 nodes)
