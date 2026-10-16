# Monte Carlo Utility Module & Integration Plan

## Overview

Create a consolidated Monte Carlo utility module (`nimcp_monte_carlo.h/c`) following the callback-based generic API pattern established by `nimcp_sort.h/c`, then integrate these algorithms into high-value cognitive modules.

### Confirmed Scope
- **All 4 new integrations**: Basal Ganglia, RCOG, Hypothesis Generation, Imagination Engine
- **Refactor existing code**: FEP Planning and Credit Assignment will use the new generic API

---

## Phase 1: Core Monte Carlo Utility Module

### Files to Create
- `include/utils/algorithms/nimcp_monte_carlo.h` ✅ CREATED
- `src/utils/algorithms/nimcp_monte_carlo.c`

### API Design

Following the callback-based pattern from `nimcp_sort.h`:

```c
// ============================================================================
// MCTS Core Structures
// ============================================================================

typedef struct mcts_node mcts_node_t;

// Callback: Get available actions from state
typedef uint32_t (*mcts_get_action_count_fn)(const void* state, void* user_data);
typedef uint32_t (*mcts_get_action_fn)(const void* state, uint32_t action_idx, void* user_data);

// Callback: Apply action to state, return new state
typedef void* (*mcts_apply_action_fn)(const void* state, uint32_t action, void* user_data);

// Callback: Evaluate state (heuristic value)
typedef float (*mcts_evaluate_fn)(const void* state, void* user_data);

// Callback: Check if state is terminal
typedef bool (*mcts_is_terminal_fn)(const void* state, void* user_data);

// Callback: Free state memory
typedef void (*mcts_free_state_fn)(void* state, void* user_data);

// Callback: Clone state
typedef void* (*mcts_clone_state_fn)(const void* state, void* user_data);

// MCTS Configuration
typedef struct mcts_config {
    uint32_t max_iterations;      // Number of MCTS iterations
    uint32_t max_depth;           // Maximum tree depth
    float exploration_constant;   // UCB exploration (default: sqrt(2))
    float discount_factor;        // Reward discount (default: 0.99)
    uint32_t max_nodes;           // Memory limit for tree

    // Callbacks
    mcts_get_action_count_fn get_action_count;
    mcts_get_action_fn get_action;
    mcts_apply_action_fn apply_action;
    mcts_evaluate_fn evaluate;
    mcts_is_terminal_fn is_terminal;
    mcts_free_state_fn free_state;
    mcts_clone_state_fn clone_state;

    void* user_data;
} mcts_config_t;

// MCTS Result
typedef struct mcts_result {
    uint32_t best_action;
    float best_value;
    uint32_t* action_visits;      // Visit counts per action
    float* action_values;         // Q-values per action
    uint32_t num_actions;
    uint32_t nodes_created;
    uint32_t iterations_completed;
} mcts_result_t;

// ============================================================================
// Monte Carlo Sampling Structures
// ============================================================================

typedef enum {
    MC_SAMPLE_UNIFORM,
    MC_SAMPLE_IMPORTANCE,
    MC_SAMPLE_STRATIFIED,
    MC_SAMPLE_METROPOLIS_HASTINGS
} mc_sampling_method_t;

// Callback: Sample from distribution
typedef float (*mc_sample_fn)(void* user_data);

// Callback: Compute weight for importance sampling
typedef float (*mc_weight_fn)(float sample, void* user_data);

// Callback: Objective function to estimate
typedef float (*mc_objective_fn)(float sample, void* user_data);

// Callback: Proposal distribution for M-H
typedef float (*mc_proposal_fn)(float current, void* user_data);

// Callback: Target density (unnormalized) for M-H
typedef float (*mc_density_fn)(float x, void* user_data);

typedef struct mc_config {
    mc_sampling_method_t method;
    uint32_t num_samples;
    uint32_t burnin;              // For MCMC methods
    float tolerance;              // Convergence tolerance

    // Callbacks (set based on method)
    mc_sample_fn sampler;
    mc_weight_fn weight;
    mc_objective_fn objective;
    mc_proposal_fn proposal;
    mc_density_fn density;

    void* user_data;
    uint32_t seed;                // RNG seed (0 = use time)
} mc_config_t;

typedef struct mc_result {
    float estimate;               // E[f(X)]
    float variance;               // Var[f(X)]
    float std_error;              // Standard error
    float* samples;               // Raw samples (if requested)
    uint32_t num_samples;
    float acceptance_rate;        // For MCMC
} mc_result_t;

// ============================================================================
// Public API
// ============================================================================

// MCTS
nimcp_error_t mcts_search(const mcts_config_t* config, void* initial_state, mcts_result_t* result);
nimcp_error_t mcts_result_free(mcts_result_t* result);

// Monte Carlo Integration/Estimation
nimcp_error_t mc_estimate(const mc_config_t* config, mc_result_t* result);
nimcp_error_t mc_result_free(mc_result_t* result);

// Utility: UCB calculation
float mcts_compute_ucb(float q_value, uint32_t visit_count, uint32_t parent_visits, float c);

// Utility: Random number generation (thread-safe)
float mc_random_uniform(uint32_t* seed);
float mc_random_normal(uint32_t* seed, float mean, float stddev);
uint32_t mc_random_choice(uint32_t* seed, const float* weights, uint32_t n);

// Utility: Fisher-Yates shuffle
void mc_shuffle(uint32_t* array, uint32_t n, uint32_t* seed);
```

### Implementation Details

1. **MCTS Tree Structure** (internal):
```c
struct mcts_node {
    uint32_t id;
    uint32_t parent_id;
    uint32_t* children_ids;
    uint32_t num_children;
    uint32_t action_from_parent;

    void* state;                  // Opaque state pointer
    uint32_t visit_count;
    float total_value;
    float q_value;

    bool is_expanded;
    bool is_terminal;
};
```

2. **Memory Management**:
   - Stack allocation for small trees (< 256 nodes)
   - Dynamic allocation with cleanup on error
   - User-provided `free_state` callback for state cleanup

3. **Selection Policy**: UCB1 with configurable exploration constant

4. **Expansion**: Full expansion (all actions) or progressive widening (configurable)

5. **Simulation**: Configurable rollout depth with user `evaluate` heuristic

6. **Backpropagation**: Standard value backup with optional discounting

---

## Phase 2: Refactor Existing MCTS Implementations

### 2.1 FEP Planning (`src/cognitive/free_energy/nimcp_fep_planning.c`)

**Current State**: Full MCTS implementation with FEP-specific node structure

**Refactor Strategy**:
1. Keep FEP-specific wrapper functions
2. Replace internal MCTS logic with calls to generic `mcts_search()`
3. Implement callbacks:
   - `fep_get_action_count()` - Return number of available actions
   - `fep_apply_action()` - Use FEP generative model for state transition
   - `fep_evaluate()` - Return `1.0 / (1.0 + prediction_error)`
   - `fep_is_terminal()` - Check if depth >= horizon

**Files to Modify**:
- `src/cognitive/free_energy/nimcp_fep_planning.c`

### 2.2 Credit Assignment (`src/cognitive/game_theory/nimcp_credit_assignment.c`)

**Current State**: Monte Carlo Shapley with Fisher-Yates shuffle

**Refactor Strategy**:
1. Move `fisher_yates_shuffle()` to `nimcp_monte_carlo.c` as `mc_shuffle()`
2. Use `mc_random_uniform()` for thread-safe RNG
3. Keep Shapley-specific logic in credit_assignment.c

**Files to Modify**:
- `src/cognitive/game_theory/nimcp_credit_assignment.c`

### 2.3 Model-Based Planning (`src/core/brain/subcortical/nimcp_bg_model_based.c`)

**Current State**: Trajectory sampling with learned transition model

**Refactor Strategy**:
1. Create wrapper to use generic MCTS for planning
2. Implement callbacks using learned transition/reward models
3. Keep model learning code unchanged

**Files to Modify**:
- `src/core/brain/subcortical/nimcp_bg_model_based.c`

---

## Phase 3: New MCTS Integrations

### 3.1 Basal Ganglia Action Selection (Priority: HIGH)

**File**: `src/core/brain/subcortical/nimcp_basal_ganglia.c`

**Integration Point**: `basal_ganglia_select_action()`

**MCTS Mapping**:
- **State**: `{cortical_input, action_values[], dopamine_level}`
- **Actions**: `action_id` (0 to num_actions-1)
- **Reward**: `dopamine_signal` or `thalamic_output[action]`
- **Terminal**: Single action selection (depth 1) or multi-step planning

**New Functions**:
```c
// Use MCTS when conflict is high (uncertain decision)
uint32_t basal_ganglia_select_action_mcts(
    basal_ganglia_t* bg,
    const float* cortical_input,
    uint32_t num_iterations
);

// Callbacks
static uint32_t bg_get_action_count(const void* state, void* user_data);
static uint32_t bg_get_action(const void* state, uint32_t idx, void* user_data);
static void* bg_apply_action(const void* state, uint32_t action, void* user_data);
static float bg_evaluate(const void* state, void* user_data);
```

**Activation Condition**: Use MCTS when `conflict_level > threshold` (uncertain decisions)

### 3.2 Recursive Cognition Task Decomposition (Priority: HIGH)

**File**: `src/cognitive/recursive/nimcp_rcog_orchestrator.c`

**Integration Point**: `rcog_orchestrator_decompose()`

**MCTS Mapping**:
- **State**: `{goal, context, current_depth, subtasks[]}`
- **Actions**: Decomposition strategies (SEQUENTIAL, PARALLEL, HIERARCHICAL, ADAPTIVE)
- **Reward**: `answer_confidence` after subtask completion
- **Terminal**: `confidence >= threshold` or `depth >= max_depth`

**New Functions**:
```c
// MCTS-guided decomposition strategy selection
rcog_decomposition_strategy_t rcog_select_strategy_mcts(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    uint32_t num_iterations
);

// Callbacks for MCTS
static uint32_t rcog_get_strategy_count(const void* state, void* user_data);
static void* rcog_apply_strategy(const void* state, uint32_t strategy, void* user_data);
static float rcog_evaluate_decomposition(const void* state, void* user_data);
```

### 3.3 Hypothesis Generation (Priority: MEDIUM)

**File**: `src/cognitive/parietal/nimcp_hypothesis_generation.c`

**Integration Point**: `hypothesis_generate_explanations()`

**MCTS Mapping**:
- **State**: `{observations[], theories[], tested_predictions[]}`
- **Actions**: Select theory to test / generate prediction
- **Reward**: `+1` if prediction confirmed, `-1` if falsified
- **Terminal**: High-confidence theory found

**New Functions**:
```c
// MCTS-guided hypothesis search
hypogen_theory_t* hypothesis_search_mcts(
    hypogen_system_t* sys,
    const hypogen_observation_t* observations,
    uint32_t num_observations,
    uint32_t num_iterations
);
```

### 3.4 Imagination Engine (Priority: MEDIUM)

**File**: `src/cognitive/imagination/nimcp_imagination_engine.c`

**Integration Point**: `imagination_step_scenario()` for goal-directed imagination

**MCTS Mapping**:
- **State**: `imagination_scenario_t` (latent tensor + goal)
- **Actions**: Transformations (rotate, translate, inject element, change mode)
- **Reward**: `goal_progress + coherence`
- **Terminal**: `goal_progress >= threshold`

**New Functions**:
```c
// MCTS-guided imagination toward goal
nimcp_error_t imagination_search_goal_mcts(
    imagination_engine_t* engine,
    imagination_scenario_t* scenario,
    const imagination_goal_t* goal,
    uint32_t num_iterations
);
```

---

## Phase 4: Unit Tests

**File**: `test/unit/utils/algorithms/test_monte_carlo.cpp`

### Test Cases

1. **MCTS Core**:
   - Simple game tree (tic-tac-toe style)
   - UCB calculation correctness
   - Tree expansion limits
   - Memory cleanup

2. **Monte Carlo Estimation**:
   - Uniform sampling (estimate pi)
   - Importance sampling (rare event estimation)
   - Metropolis-Hastings (sample from distribution)
   - Convergence with increasing samples

3. **Utilities**:
   - `mc_shuffle()` uniformity test
   - `mc_random_uniform()` distribution test
   - `mc_random_normal()` mean/variance test

---

## Implementation Order

| Step | Task | Files | Status |
|------|------|-------|--------|
| 1 | Create `nimcp_monte_carlo.h` header | `include/utils/algorithms/nimcp_monte_carlo.h` | ✅ DONE |
| 2 | Implement MCTS core | `src/utils/algorithms/nimcp_monte_carlo.c` | ✅ DONE |
| 3 | Implement MC sampling | `src/utils/algorithms/nimcp_monte_carlo.c` | ✅ DONE |
| 4 | Implement utilities | `src/utils/algorithms/nimcp_monte_carlo.c` | ✅ DONE |
| 5 | Update CMakeLists.txt | `src/lib/CMakeLists.txt` | ✅ DONE |
| 6 | Create unit tests | `test/unit/utils/algorithms/test_monte_carlo.cpp` | ✅ DONE (33 tests) |
| 7 | Build and verify | - | ✅ DONE (all tests pass) |
| 8 | Refactor FEP planning | `src/cognitive/free_energy/nimcp_fep_planning.c` | ⏸️ DEFERRED (tightly coupled) |
| 9 | Refactor credit assignment | `src/cognitive/game_theory/nimcp_credit_assignment.c` | ✅ DONE (mc_shuffle_u32) |
| 10 | Refactor BG model-based | `src/core/brain/subcortical/nimcp_bg_model_based.c` | ✅ DONE (mc_random_*) |
| 11 | Integrate BG action selection | `src/core/brain/subcortical/nimcp_basal_ganglia.c` | ✅ DONE |
| 12 | Integrate RCOG decomposition | `src/cognitive/recursive/nimcp_rcog_orchestrator.c` | ✅ DONE |
| 13 | Integrate hypothesis generation | `src/cognitive/parietal/nimcp_hypothesis_generation.c` | ✅ DONE |
| 14 | Integrate imagination engine | `src/cognitive/imagination/nimcp_imagination_engine.c` | ✅ DONE |
| 15 | Final build and test | - | ✅ DONE |

---

## Critical Files Summary

### New Files
- `include/utils/algorithms/nimcp_monte_carlo.h` ✅
- `src/utils/algorithms/nimcp_monte_carlo.c`
- `test/unit/utils/algorithms/test_monte_carlo.cpp`

### Modified Files
- `src/lib/CMakeLists.txt` (add source)
- `src/cognitive/free_energy/nimcp_fep_planning.c` (refactor)
- `src/cognitive/game_theory/nimcp_credit_assignment.c` (refactor)
- `src/core/brain/subcortical/nimcp_bg_model_based.c` (refactor)
- `src/core/brain/subcortical/nimcp_basal_ganglia.c` (new integration)
- `src/cognitive/recursive/nimcp_rcog_orchestrator.c` (new integration)
- `src/cognitive/parietal/nimcp_hypothesis_generation.c` (new integration)
- `src/cognitive/imagination/nimcp_imagination_engine.c` (new integration)

---

## Design Decisions

1. **Callback-based API**: Enables generic MCTS that works with any state representation
2. **Separate MCTS and MC sampling**: Different use cases, cleaner separation
3. **Thread-safe RNG**: Use `rand_r()` style with seed parameter
4. **Stack allocation for small trees**: Optimize common case (< 256 nodes)
5. **Progressive integration**: Refactor existing code before new integrations
6. **Preserve existing APIs**: Wrapper functions maintain backward compatibility
