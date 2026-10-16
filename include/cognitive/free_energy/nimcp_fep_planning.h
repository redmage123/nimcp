/**
 * @file nimcp_fep_planning.h
 * @brief Multi-step MCTS Planning Module for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Monte Carlo Tree Search (MCTS) planning integrated with FEP for multi-step
 *       action selection and trajectory optimization.
 * WHY:  Single-step active inference is limited for complex tasks requiring planning
 *       ahead. MCTS enables forward simulation using the generative model to find
 *       optimal action sequences that minimize expected free energy over horizons.
 * HOW:  Implement MCTS (selection, expansion, simulation, backpropagation) using FEP's
 *       generative model for state transitions and EFE as the value function.
 *
 * BIOLOGICAL FOUNDATION:
 * ==================================================================================
 *
 * PREFRONTAL CORTEX - Multi-step Planning:
 * -----------------------------------------
 * The dorsolateral prefrontal cortex (dlPFC) maintains goal representations and
 * orchestrates action sequences. Neural activity persists during delay periods,
 * representing future states and planned actions.
 *
 * HIPPOCAMPAL REPLAY - Trajectory Simulation:
 * --------------------------------------------
 * During rest and sleep, hippocampal place cells "replay" sequences of positions,
 * simulating future trajectories. This replay is essential for planning novel routes
 * and evaluating alternatives before execution.
 *
 * MODEL-BASED RL IN THE BRAIN:
 * -----------------------------
 * The brain combines model-free (habitual) and model-based (goal-directed) systems:
 *   - Model-free: Cached action values (dorsal striatum)
 *   - Model-based: Forward simulation using world model (prefrontal-hippocampal)
 *
 * FEP PLANNING FRAMEWORK:
 * -----------------------
 * Active inference extends to planning by evaluating policies (action sequences)
 * based on their expected free energy over future time steps:
 *
 *   G(π) = Σ_τ E_q(o_τ,s_τ|π) [ln q(s_τ|π) - ln p(o_τ,s_τ|C)]
 *
 * Where:
 *   π = Policy (action sequence)
 *   τ = Time step in the future
 *   C = Prior preferences (goals)
 *   G(π) = Expected free energy (lower is better)
 *
 * MCTS FOR FEP:
 * -------------
 * Monte Carlo Tree Search explores the space of action sequences:
 *
 * 1. SELECTION (Upper Confidence Bound):
 *    UCB(s,a) = Q(s,a) + c * sqrt(ln(N(s)) / N(s,a))
 *    - Q(s,a) = Average value (negative EFE)
 *    - N(s), N(s,a) = Visit counts
 *    - c = Exploration constant
 *
 * 2. EXPANSION:
 *    Add new action node to the tree when a leaf is reached
 *
 * 3. SIMULATION (Rollout):
 *    Use FEP's generative model to simulate future:
 *    s' = f(s, a) + η    (state transition with noise)
 *    o' = g(s') + ε      (observation generation)
 *    G = -EFE(s', o')    (value from FEP)
 *
 * 4. BACKPROPAGATION:
 *    Update Q-values and visit counts back up the tree
 *
 * TREE POLICY vs ROLLOUT POLICY:
 * -------------------------------
 * - Tree policy: UCB selection (exploration-exploitation)
 * - Rollout policy: Use FEP's action selection or random
 *
 * PLANNING HORIZONS:
 * ------------------
 * Biological planning has limited depth due to:
 *   - Computational cost (exponential branching)
 *   - Uncertainty accumulation
 *   - Working memory capacity (7±2 items)
 *
 * Typical horizons: 3-10 steps
 *
 * REFERENCES:
 * - Silver et al. (2016) "Mastering the game of Go with deep neural networks"
 * - Browne et al. (2012) "A Survey of Monte Carlo Tree Search Methods"
 * - Friston et al. (2017) "Active inference and learning" (planning as inference)
 * - Hassabis et al. (2017) "Neuroscience-Inspired AI" (hippocampal replay)
 * - Daw et al. (2005) "Uncertainty-based competition between prefrontal and dorsolateral"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP MCTS PLANNING SYSTEM                                ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   MCTS SEARCH TREE                                  │  ║
 * ║   │                                                                     │  ║
 * ║   │                    Root (current state)                            │  ║
 * ║   │                    /      |      \                                 │  ║
 * ║   │               a1         a2        a3                              │  ║
 * ║   │              /  \       / | \      / \                             │  ║
 * ║   │            s1   s2    s3 s4 s5   s6 s7                             │  ║
 * ║   │           N=12      N=8        N=5                                 │  ║
 * ║   │         Q=-2.1    Q=-1.8     Q=-3.2                                │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   MCTS ITERATION                                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   1. SELECT: UCB traversal to leaf                                 │  ║
 * ║   │      → Use UCB = Q + c*sqrt(ln(N_parent)/N_child)                  │  ║
 * ║   │                                                                     │  ║
 * ║   │   2. EXPAND: Add new child nodes                                   │  ║
 * ║   │      → Create nodes for available actions                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   3. SIMULATE: Rollout using FEP                                   │  ║
 * ║   │      → s' = f(s,a) (generative model)                             │  ║
 * ║   │      → V = -EFE(s',o') (FEP value)                                │  ║
 * ║   │                                                                     │  ║
 * ║   │   4. BACKPROP: Update Q and N                                      │  ║
 * ║   │      → Q = (Q*N + V)/(N+1)                                        │  ║
 * ║   │      → N = N + 1                                                   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   FEP INTEGRATION                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │   Generative Model: State transitions f(s,a)                       │  ║
 * ║   │   Value Function: V = -G(π) where G = EFE                          │  ║
 * ║   │   Preferred Observations: Goal encoding p(o|C)                     │  ║
 * ║   │   Uncertainty: Track epistemic uncertainty via beliefs             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FEP_PLANNING_H
#define NIMCP_FEP_PLANNING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Planning limits */
#define FEP_PLANNING_MAX_HORIZON       32       /**< Maximum planning horizon */
#define FEP_PLANNING_MAX_SIMULATIONS   10000    /**< Maximum MCTS simulations */
#define FEP_PLANNING_MAX_TREE_NODES    100000   /**< Maximum tree nodes */
#define FEP_PLANNING_MAX_BEAM_WIDTH    64       /**< Maximum beam width */
#define FEP_PLANNING_MAX_ACTIONS       256      /**< Maximum action branching */

/* Default parameters */
#define FEP_PLANNING_DEFAULT_HORIZON          5        /**< Default planning steps */
#define FEP_PLANNING_DEFAULT_SIMULATIONS      100      /**< Default MCTS simulations */
#define FEP_PLANNING_DEFAULT_EXPLORATION      1.414f   /**< Default UCB constant (sqrt(2)) */
#define FEP_PLANNING_DEFAULT_DISCOUNT         0.95f    /**< Default discount factor */
#define FEP_PLANNING_DEFAULT_PRUNING          -10.0f   /**< Default pruning threshold */
#define FEP_PLANNING_DEFAULT_BEAM_WIDTH       8        /**< Default beam width */

/* Bio-async */
#define FEP_PLANNING_BIO_INBOX_SIZE   64      /**< Bio-async inbox capacity */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Planning method
 *
 * WHAT: Different planning algorithms
 * WHY:  Trade-offs between optimality, speed, and memory
 * HOW:  Enum-based selection
 */
typedef enum {
    PLANNING_MCTS = 0,           /**< Monte Carlo Tree Search (best for large spaces) */
    PLANNING_BEAM_SEARCH,        /**< Beam search with pruning (memory efficient) */
    PLANNING_DYNAMIC_PROG,       /**< Dynamic programming (optimal for small spaces) */
    PLANNING_ROLLOUT             /**< Simple rollout policy (fast, suboptimal) */
} fep_planning_method_t;

/**
 * @brief MCTS node state
 */
typedef enum {
    MCTS_NODE_UNVISITED = 0,     /**< Never visited */
    MCTS_NODE_EXPANDED,          /**< Children created */
    MCTS_NODE_TERMINAL           /**< Terminal state (goal or horizon) */
} mcts_node_state_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief MCTS tree node
 *
 * WHAT: Single node in the MCTS search tree
 * WHY:  Store state, action, statistics for UCB
 * HOW:  Linked structure with visit counts and Q-values
 */
typedef struct mcts_node {
    uint32_t node_id;             /**< Unique node identifier */
    uint32_t parent_id;           /**< Parent node (0 for root) */
    uint32_t* children_ids;       /**< Child node IDs */
    uint32_t num_children;        /**< Number of children */

    /* State information */
    float* state;                 /**< Belief state at this node */
    uint32_t state_dim;           /**< State dimensionality */
    uint32_t depth;               /**< Depth in tree (0 = root) */

    /* Action that led here */
    uint32_t action_id;           /**< Action from parent (0 for root) */

    /* MCTS statistics */
    uint32_t visit_count;         /**< N(s) - number of visits */
    float total_value;            /**< Sum of rollout values */
    float q_value;                /**< Q(s,a) = total_value / visit_count */
    float prior_prob;             /**< Prior probability (for AlphaGo-style) */

    /* FEP-specific */
    float expected_free_energy;   /**< EFE at this state */
    float uncertainty;            /**< Epistemic uncertainty */

    /* Node state */
    mcts_node_state_t state_type;
} mcts_node_t;

/**
 * @brief Action plan
 *
 * WHAT: Sequence of actions and expected outcomes
 * WHY:  Result of planning - what to execute
 * HOW:  Ordered action IDs with value estimate
 */
typedef struct {
    uint32_t* action_sequence;    /**< Action IDs */
    size_t sequence_length;       /**< Number of actions */
    float expected_value;         /**< Expected cumulative value */
    float uncertainty;            /**< Plan uncertainty */

    /* Per-step information */
    float* step_values;           /**< Value at each step */
    float* step_efe;              /**< EFE at each step */
} fep_plan_t;

/**
 * @brief Planning statistics
 */
typedef struct {
    uint64_t plans_generated;     /**< Total plans generated */
    uint64_t simulations_run;     /**< Total MCTS simulations */
    uint64_t nodes_created;       /**< Total tree nodes created */
    uint64_t nodes_pruned;        /**< Nodes pruned */
    float avg_plan_length;        /**< Average plan length */
    float avg_expected_value;     /**< Average plan value */
    float avg_tree_depth;         /**< Average tree depth */
    float avg_branching_factor;   /**< Average branching factor */
    uint32_t cache_hits;          /**< State cache hits */
    uint32_t cache_misses;        /**< State cache misses */
} fep_planning_stats_t;

/**
 * @brief Planning configuration
 */
typedef struct {
    /* Planning method */
    fep_planning_method_t method;

    /* Horizon settings */
    uint32_t planning_horizon;    /**< Steps to look ahead */
    uint32_t num_simulations;     /**< MCTS simulations per planning call */

    /* MCTS parameters */
    float exploration_constant;   /**< UCB exploration term (c) */
    float discount_factor;        /**< Temporal discounting (γ) */
    bool use_prior;               /**< Use prior probabilities (AlphaGo-style) */

    /* Beam search parameters */
    uint32_t beam_width;          /**< Number of best paths to keep */
    float pruning_threshold;      /**< Prune branches with value < threshold */

    /* Optimization */
    bool enable_caching;          /**< Cache state evaluations */
    bool enable_parallel_rollouts; /**< Parallel simulations (future) */
    bool enable_progressive_widening; /**< Progressive widening for continuous actions */

    /* Termination */
    float convergence_threshold;  /**< Stop if value converges */
    uint32_t max_tree_nodes;      /**< Maximum tree size */
} fep_planning_config_t;

/**
 * @brief Complete FEP planning system
 */
typedef struct fep_planning_system {
    /* Configuration */
    fep_planning_config_t config;

    /* FEP integration */
    fep_system_t* fep;            /**< Connected FEP system */
    bool fep_connected;

    /* MCTS tree */
    mcts_node_t** tree_nodes;     /**< Array of tree nodes */
    uint32_t num_nodes;           /**< Current number of nodes */
    uint32_t max_nodes;           /**< Maximum capacity */
    uint32_t root_id;             /**< Current root node ID */

    /* State cache (for performance) */
    void* state_cache;            /**< Hash table of state evaluations */

    /* Current plan */
    fep_plan_t current_plan;      /**< Most recent plan */
    uint32_t plan_step;           /**< Current execution step */

    /* Statistics */
    fep_planning_stats_t stats;

    /* Bio-async integration */
    bio_module_context_t bio_ctx; /**< Bio-async context */
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;
} fep_planning_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default planning configuration
 *
 * WHAT: Provide sensible defaults for planning
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Set horizon ~5 steps (working memory limit), exploration ~sqrt(2)
 *
 * @param config Output configuration
 */
void fep_planning_default_config(fep_planning_config_t* config);

/**
 * @brief Create FEP planning system
 *
 * WHAT: Initialize MCTS planning system
 * WHY:  Enable multi-step planning for complex tasks
 * HOW:  Allocate tree structure, initialize statistics
 *
 * @param config Planning configuration
 * @return New planning system or NULL on failure
 */
fep_planning_system_t* fep_planning_create(const fep_planning_config_t* config);

/**
 * @brief Destroy planning system
 *
 * WHAT: Clean up all planning resources
 * WHY:  Prevent memory leaks
 * HOW:  Free tree nodes, cache, plan memory
 *
 * @param sys Planning system (NULL safe)
 */
void fep_planning_destroy(fep_planning_system_t* sys);

/**
 * @brief Reset planning system
 *
 * WHAT: Clear tree and restart planning
 * WHY:  Start fresh after goal change or environment shift
 * HOW:  Free all nodes, reset statistics
 *
 * @param sys Planning system
 * @return 0 on success, error code otherwise
 */
int fep_planning_reset(fep_planning_system_t* sys);

/* ============================================================================
 * FEP Integration API
 * ============================================================================ */

/**
 * @brief Connect to FEP system
 *
 * WHAT: Link planning system to FEP for state transitions and values
 * WHY:  Planning needs generative model and EFE computation
 * HOW:  Store FEP pointer, validate dimensions
 *
 * @param planning Planning system
 * @param fep FEP system to connect
 * @return 0 on success
 */
int fep_planning_connect(fep_planning_system_t* planning, fep_system_t* fep);

/**
 * @brief Disconnect from FEP system
 *
 * @param planning Planning system
 * @return 0 on success
 */
int fep_planning_disconnect(fep_planning_system_t* planning);

/* ============================================================================
 * Planning API
 * ============================================================================ */

/**
 * @brief Generate action plan from current state
 *
 * WHAT: Run MCTS to find optimal action sequence
 * WHY:  Core planning function - find best path to goal
 * HOW:  Execute MCTS iterations (select, expand, simulate, backprop)
 *
 * @param sys Planning system
 * @param fep FEP system (can override connected one)
 * @param current_state Current belief state
 * @param state_dim State dimensionality
 * @param plan Output plan structure
 * @return 0 on success
 */
int fep_planning_generate_plan(
    fep_planning_system_t* sys,
    fep_system_t* fep,
    const float* current_state,
    size_t state_dim,
    fep_plan_t* plan
);

/**
 * @brief Evaluate existing plan
 *
 * WHAT: Compute expected value of action sequence
 * WHY:  Assess plan quality or compare alternatives
 * HOW:  Simulate plan execution using FEP generative model
 *
 * @param sys Planning system
 * @param fep FEP system
 * @param plan Plan to evaluate
 * @param value Output expected value
 * @return 0 on success
 */
int fep_planning_evaluate_plan(
    fep_planning_system_t* sys,
    fep_system_t* fep,
    const fep_plan_t* plan,
    float* value
);

/**
 * @brief Replan from new state
 *
 * WHAT: Generate new plan given state change
 * WHY:  Environment changed or action failed - adapt
 * HOW:  Can reuse tree (move root) or restart
 *
 * @param sys Planning system
 * @param fep FEP system
 * @param new_state Updated state
 * @param state_dim State dimensionality
 * @param plan Output new plan
 * @return 0 on success
 */
int fep_planning_replan(
    fep_planning_system_t* sys,
    fep_system_t* fep,
    const float* new_state,
    size_t state_dim,
    fep_plan_t* plan
);

/* ============================================================================
 * MCTS-Specific API
 * ============================================================================ */

/**
 * @brief MCTS selection phase
 *
 * WHAT: Traverse tree using UCB to select best leaf
 * WHY:  Balance exploration vs exploitation
 * HOW:  UCB(s,a) = Q(s,a) + c * sqrt(ln(N_parent) / N_child)
 *
 * @param sys Planning system
 * @param node_id Starting node ID
 * @param action Output selected action
 * @return Selected child node ID, or 0 if terminal
 */
int fep_mcts_select(fep_planning_system_t* sys, uint32_t node_id, uint32_t* action);

/**
 * @brief MCTS expansion phase
 *
 * WHAT: Add new child nodes for unvisited actions
 * WHY:  Grow tree to explore new possibilities
 * HOW:  Create child nodes for each valid action
 *
 * @param sys Planning system
 * @param node_id Node to expand
 * @return Number of children added, or -1 on error
 */
int fep_mcts_expand(fep_planning_system_t* sys, uint32_t node_id);

/**
 * @brief MCTS simulation phase (rollout)
 *
 * WHAT: Simulate trajectory from node to horizon using FEP
 * WHY:  Estimate value without full tree expansion
 * HOW:  Apply FEP's generative model and action selection
 *
 * @param sys Planning system
 * @param fep FEP system
 * @param node_id Starting node for rollout
 * @param value Output rollout value
 * @return 0 on success
 */
int fep_mcts_simulate(
    fep_planning_system_t* sys,
    fep_system_t* fep,
    uint32_t node_id,
    float* value
);

/**
 * @brief MCTS backpropagation phase
 *
 * WHAT: Update Q-values and visit counts up the tree
 * WHY:  Propagate value estimates to ancestors
 * HOW:  Q_new = (Q_old * N + V) / (N + 1), N = N + 1
 *
 * @param sys Planning system
 * @param node_id Leaf node to backpropagate from
 * @param value Rollout value
 * @return 0 on success
 */
int fep_mcts_backpropagate(fep_planning_system_t* sys, uint32_t node_id, float value);

/**
 * @brief Get best child using visit count
 *
 * WHAT: Select child with highest visit count (most robust)
 * WHY:  After MCTS converges, most-visited is best
 * HOW:  Argmax over N(s,a)
 *
 * @param sys Planning system
 * @param node_id Parent node
 * @return Best child node ID, or 0 if none
 */
uint32_t fep_mcts_get_best_child(const fep_planning_system_t* sys, uint32_t node_id);

/* ============================================================================
 * Plan Management API
 * ============================================================================ */

/**
 * @brief Create plan structure
 *
 * WHAT: Allocate memory for action plan
 * WHY:  Prepare to store planning results
 * HOW:  Allocate arrays for actions and per-step values
 *
 * @param plan Plan to initialize
 * @param max_length Maximum plan length
 * @return 0 on success
 */
int fep_plan_create(fep_plan_t* plan, size_t max_length);

/**
 * @brief Destroy plan structure
 *
 * WHAT: Free plan memory
 * WHY:  Clean up after planning
 * HOW:  Free all arrays
 *
 * @param plan Plan to destroy (NULL safe)
 */
void fep_plan_destroy(fep_plan_t* plan);

/**
 * @brief Get next action from plan
 *
 * WHAT: Extract action at specific step
 * WHY:  Execute plan step-by-step
 * HOW:  Index into action sequence
 *
 * @param plan Plan to query
 * @param step Step index (0-based)
 * @param action Output action ID
 * @return 0 on success, -1 if step out of bounds
 */
int fep_plan_get_next_action(const fep_plan_t* plan, uint32_t step, uint32_t* action);

/**
 * @brief Copy plan
 *
 * @param dest Destination plan
 * @param src Source plan
 * @return 0 on success
 */
int fep_plan_copy(fep_plan_t* dest, const fep_plan_t* src);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get planning statistics
 *
 * @param sys Planning system
 * @param stats Output statistics
 * @return 0 on success
 */
int fep_planning_get_stats(const fep_planning_system_t* sys, fep_planning_stats_t* stats);

/**
 * @brief Get current tree size
 *
 * @param sys Planning system
 * @return Number of nodes in tree
 */
uint32_t fep_planning_get_tree_size(const fep_planning_system_t* sys);

/**
 * @brief Get tree depth
 *
 * @param sys Planning system
 * @return Maximum depth of tree
 */
uint32_t fep_planning_get_tree_depth(const fep_planning_system_t* sys);

/**
 * @brief Check if plan is valid
 *
 * @param plan Plan to check
 * @return true if plan has actions
 */
bool fep_plan_is_valid(const fep_plan_t* plan);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register planning system with bio-async messaging
 * WHY:  Enable inter-module communication for planning
 * HOW:  Register as BIO_MODULE_FEP_PLANNING
 *
 * @param sys Planning system
 * @return 0 on success
 */
int fep_planning_connect_bio_async(fep_planning_system_t* sys);

/**
 * @brief Disconnect from bio-async router
 *
 * @param sys Planning system
 * @return 0 on success
 */
int fep_planning_disconnect_bio_async(fep_planning_system_t* sys);

/**
 * @brief Check if bio-async is connected
 *
 * @param sys Planning system
 * @return true if connected
 */
bool fep_planning_is_bio_async_connected(const fep_planning_system_t* sys);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert planning method to string
 *
 * @param method Planning method
 * @return Human-readable string
 */
const char* fep_planning_method_to_string(fep_planning_method_t method);

/**
 * @brief Convert MCTS node state to string
 *
 * @param state Node state
 * @return Human-readable string
 */
const char* fep_planning_node_state_to_string(mcts_node_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_PLANNING_H */
