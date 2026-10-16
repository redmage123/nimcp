/**
 * @file nimcp_rcog_orchestrator.h
 * @brief Recursive Cognition Orchestrator - Task Decomposition and Coordination
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Coordinates task decomposition and manages the recursive call tree
 * WHY:  Maps to RLM's "root model" that coordinates sub-workers without direct tools
 * HOW:  Decomposes goals into subtasks, schedules execution, aggregates results
 *
 * BIOLOGICAL BASIS:
 * The orchestrator models prefrontal cortex executive functions:
 * - Goal decomposition: Breaking complex tasks into manageable subtasks
 * - Working memory management: Tracking active subtasks and dependencies
 * - Attention allocation: Prioritizing subtasks based on relevance
 * - Recursion control: Preventing unbounded processing depth
 *
 * ARCHITECTURE:
 * ```
 * +----------------------+                    +----------------------+
 * |    RCOG ENGINE       |                    |    ORCHESTRATOR      |
 * |                      |                    |                      |
 * | rcog_process(goal)   |---> decompose ---->|  Strategy Selection  |
 * |                      |                    |  (sequential/parallel|
 * |                      |                    |   /hierarchical)     |
 * |                      |<--- subtasks <-----|                      |
 * +----------------------+                    +----------------------+
 *          |                                           |
 *          v                                           v
 * +----------------------+                    +----------------------+
 * |  DELEGATION POOL     |                    |  ANSWER REFINER      |
 * |  (execute subtasks)  |<--- dispatch ----->|  (aggregate results) |
 * +----------------------+                    +----------------------+
 * ```
 *
 * KEY CONCEPTS:
 * - Decomposition: Split goals into smaller, parallelizable subtasks
 * - Dependency Graph: DAG of subtask dependencies for correct ordering
 * - Depth Limiting: Prevent infinite recursion with configurable limits
 * - Early Termination: Stop when answer confidence exceeds threshold
 * - Adaptive Strategy: Choose decomposition based on goal type and context
 */

#ifndef NIMCP_RCOG_ORCHESTRATOR_H
#define NIMCP_RCOG_ORCHESTRATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/recursive/nimcp_rcog_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * FORWARD DECLARATIONS
 *===========================================================================*/

struct rcog_context_store;
struct rcog_answer_refiner;
struct rcog_delegation_pool;
struct rcog_engine;
struct rcog_imagination_bridge;
struct rcog_immune_bridge;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum subtasks per decomposition */
#define RCOG_ORCH_MAX_SUBTASKS_PER_DECOMP 32

/** Maximum dependency edges */
#define RCOG_ORCH_MAX_DEPENDENCIES 64

/** Default decomposition timeout (ms) */
#define RCOG_ORCH_DEFAULT_DECOMP_TIMEOUT_MS 5000

/** Maximum active decompositions */
#define RCOG_ORCH_MAX_ACTIVE_DECOMPOSITIONS 16

/** Default priority for subtasks */
#define RCOG_ORCH_DEFAULT_PRIORITY 0.5f

/*=============================================================================
 * DEPENDENCY GRAPH
 *===========================================================================*/

/**
 * @brief Dependency edge between subtasks
 */
typedef struct {
    uint64_t from_task_id;       /**< Task that must complete first */
    uint64_t to_task_id;         /**< Task that depends on completion */
    float weight;                /**< Dependency weight (for priority) */
} rcog_dependency_edge_t;

/**
 * @brief Dependency graph for subtask ordering
 */
typedef struct {
    rcog_dependency_edge_t edges[RCOG_ORCH_MAX_DEPENDENCIES];
    size_t num_edges;
    bool has_cycle;              /**< True if cycle detected (error) */
} rcog_dependency_graph_t;

/*=============================================================================
 * DECOMPOSITION
 *===========================================================================*/

/**
 * @brief Decomposition metadata
 */
typedef struct {
    uint64_t decomp_id;          /**< Unique decomposition ID */
    uint64_t parent_task_id;     /**< Parent task (0 if root) */
    uint32_t depth;              /**< Current recursion depth */
    rcog_decomposition_strategy_t strategy; /**< Strategy used */
    float estimated_complexity;  /**< Estimated complexity [0-1] */
    uint64_t created_ms;         /**< Creation timestamp */
} rcog_decomposition_metadata_t;

/**
 * @brief Complete decomposition result
 */
typedef struct {
    rcog_decomposition_metadata_t metadata;
    rcog_subtask_t* subtasks;    /**< Array of subtasks (owned) */
    size_t num_subtasks;
    rcog_dependency_graph_t* deps; /**< Dependency graph (owned, may be NULL) */
    bool ready_for_dispatch;     /**< True if decomposition is complete */
} rcog_decomposition_t;

/*=============================================================================
 * STRATEGY HINTS
 *===========================================================================*/

/**
 * @brief Hints for decomposition strategy selection
 */
typedef struct {
    bool prefer_parallel;        /**< Prefer parallel over sequential */
    bool prefer_shallow;         /**< Prefer shallow decomposition */
    uint32_t max_subtasks;       /**< Maximum subtasks to create */
    float min_subtask_complexity; /**< Minimum complexity per subtask */
    bool enable_imagination;     /**< Use imagination for strategy selection */
    bool enable_swarm;           /**< Allow swarm distribution */
} rcog_strategy_hints_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Orchestrator configuration
 */
typedef struct {
    /* Recursion control */
    uint32_t max_recursion_depth;    /**< Max depth (default: 16) */
    uint32_t max_parallel_subtasks;  /**< Max concurrent (default: 8) */

    /* Answer thresholds */
    float ready_threshold;           /**< Confidence for ready (default: 0.95) */
    uint32_t max_refinement_steps;   /**< Max iterations (default: 32) */
    bool enable_early_termination;   /**< Stop when confident */

    /* Scheduling */
    rcog_scheduling_policy_t scheduling_policy;
    uint32_t decomposition_timeout_ms;

    /* Strategy */
    rcog_decomposition_strategy_t default_strategy;
    bool enable_adaptive_strategy;   /**< Auto-select strategy */

    /* Integration */
    bool enable_imagination_planning; /**< Use imagination for planning */
    bool enable_immune_modulation;    /**< Respect immune system limits */

    /* Debug */
    bool enable_trace;               /**< Record execution trace */
    bool verbose_logging;
} rcog_orchestrator_config_t;

/*=============================================================================
 * ORCHESTRATOR STATISTICS
 *===========================================================================*/

/**
 * @brief Orchestrator statistics
 */
typedef struct {
    uint64_t goals_decomposed;
    uint64_t subtasks_created;
    uint64_t subtasks_dispatched;
    uint64_t subtasks_completed;
    uint64_t subtasks_failed;
    uint64_t depth_limit_hits;
    uint64_t early_terminations;
    uint64_t strategy_adaptations;
    uint32_t max_depth_reached;      /**< Maximum depth observed */
    float avg_subtasks_per_goal;
    float avg_depth;
    float avg_completion_time_ms;
    uint64_t total_processing_time_ms;
} rcog_orchestrator_stats_t;

/*=============================================================================
 * EXECUTION TRACE
 *===========================================================================*/

/**
 * @brief Trace entry for debugging
 */
typedef struct {
    uint64_t timestamp_ms;
    uint64_t task_id;
    const char* event;           /**< Event description */
    uint32_t depth;
    float confidence;
} rcog_trace_entry_t;

/**
 * @brief Execution trace
 */
typedef struct {
    rcog_trace_entry_t* entries;
    size_t num_entries;
    size_t capacity;
} rcog_trace_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Get default orchestrator configuration
 * @return Default configuration
 */
rcog_orchestrator_config_t rcog_orchestrator_default_config(void);

/**
 * @brief Create orchestrator with configuration
 * @param config Configuration (NULL for defaults)
 * @return Orchestrator handle or NULL on error
 */
rcog_orchestrator_t* rcog_orchestrator_create(
    const rcog_orchestrator_config_t* config
);

/**
 * @brief Create orchestrator with default configuration
 * @return Orchestrator handle or NULL on error
 */
rcog_orchestrator_t* rcog_orchestrator_create_default(void);

/**
 * @brief Destroy orchestrator and free resources
 * @param orch Orchestrator handle (NULL safe)
 */
void rcog_orchestrator_destroy(rcog_orchestrator_t* orch);

/*=============================================================================
 * CONNECTION
 *===========================================================================*/

/**
 * @brief Connect to context store for variable access
 * @param orch Orchestrator handle
 * @param store Context store handle
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_connect_context_store(
    rcog_orchestrator_t* orch,
    struct rcog_context_store* store
);

/**
 * @brief Connect to answer refiner for result aggregation
 * @param orch Orchestrator handle
 * @param refiner Answer refiner handle
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_connect_answer_refiner(
    rcog_orchestrator_t* orch,
    struct rcog_answer_refiner* refiner
);

/**
 * @brief Connect to delegation pool for subtask execution
 * @param orch Orchestrator handle
 * @param pool Delegation pool handle
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_connect_delegation_pool(
    rcog_orchestrator_t* orch,
    struct rcog_delegation_pool* pool
);

/**
 * @brief Connect to imagination bridge for planning
 * @param orch Orchestrator handle
 * @param imagination Imagination bridge handle
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_connect_imagination(
    rcog_orchestrator_t* orch,
    struct rcog_imagination_bridge* imagination
);

/**
 * @brief Connect to immune bridge for modulation
 * @param orch Orchestrator handle
 * @param immune Immune bridge handle
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_connect_immune(
    rcog_orchestrator_t* orch,
    struct rcog_immune_bridge* immune
);

/**
 * @brief Connect to parent engine
 * @param orch Orchestrator handle
 * @param engine Engine handle
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_connect_engine(
    rcog_orchestrator_t* orch,
    struct rcog_engine* engine
);

/*=============================================================================
 * DECOMPOSITION
 *===========================================================================*/

/**
 * @brief Decompose a goal into subtasks
 *
 * This is the core function that breaks a high-level goal into manageable
 * subtasks. The decomposition strategy is selected based on configuration
 * and goal characteristics.
 *
 * @param orch Orchestrator handle
 * @param goal Goal to decompose
 * @param context Context store for variable access
 * @param result Output decomposition result (caller owns)
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_decompose(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_t* result
);

/**
 * @brief Decompose with explicit strategy
 * @param orch Orchestrator handle
 * @param goal Goal to decompose
 * @param context Context store
 * @param strategy Decomposition strategy to use
 * @param result Output decomposition result
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_decompose_with_strategy(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_strategy_t strategy,
    rcog_decomposition_t* result
);

/**
 * @brief Decompose with hints for strategy selection
 * @param orch Orchestrator handle
 * @param goal Goal to decompose
 * @param context Context store
 * @param hints Strategy hints
 * @param result Output decomposition result
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_decompose_with_hints(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    const rcog_strategy_hints_t* hints,
    rcog_decomposition_t* result
);

/**
 * @brief Free decomposition result
 * @param decomp Decomposition to free
 */
void rcog_orchestrator_free_decomposition(rcog_decomposition_t* decomp);

/*=============================================================================
 * DISPATCH
 *===========================================================================*/

/**
 * @brief Dispatch decomposition to delegation pool
 *
 * Submits all ready subtasks to the delegation pool for execution.
 * Respects dependency ordering.
 *
 * @param orch Orchestrator handle
 * @param decomp Decomposition to dispatch
 * @param handle Output batch handle for tracking
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_dispatch(
    rcog_orchestrator_t* orch,
    const rcog_decomposition_t* decomp,
    rcog_batch_handle_t** handle
);

/**
 * @brief Dispatch single subtask
 * @param orch Orchestrator handle
 * @param subtask Subtask to dispatch
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_dispatch_subtask(
    rcog_orchestrator_t* orch,
    rcog_subtask_t* subtask
);

/**
 * @brief Wait for batch completion
 * @param orch Orchestrator handle
 * @param handle Batch handle
 * @param timeout_ms Timeout in milliseconds (0 = infinite)
 * @return 0 on success, error code on failure/timeout
 */
int rcog_orchestrator_await_batch(
    rcog_orchestrator_t* orch,
    rcog_batch_handle_t* handle,
    uint32_t timeout_ms
);

/*=============================================================================
 * AGGREGATION
 *===========================================================================*/

/**
 * @brief Aggregate subtask results into answer state
 *
 * Collects completed subtask results and updates the answer state.
 * Uses answer refiner for diffusion-style refinement.
 *
 * @param orch Orchestrator handle
 * @param handle Batch handle with results
 * @param results Array of subtask results
 * @param num_results Number of results
 * @param answer Answer state to update
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_aggregate(
    rcog_orchestrator_t* orch,
    rcog_batch_handle_t* handle,
    const rcog_subtask_result_t* results,
    size_t num_results,
    rcog_answer_state_t* answer
);

/**
 * @brief Refine answer state (single diffusion step)
 * @param orch Orchestrator handle
 * @param answer Answer state to refine
 * @param context Context store for additional queries
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_refine(
    rcog_orchestrator_t* orch,
    rcog_answer_state_t* answer,
    const struct rcog_context_store* context
);

/**
 * @brief Check if answer is ready
 * @param orch Orchestrator handle
 * @param answer Answer state to check
 * @return true if answer meets ready threshold
 */
bool rcog_orchestrator_is_answer_ready(
    const rcog_orchestrator_t* orch,
    const rcog_answer_state_t* answer
);

/*=============================================================================
 * DEPTH CONTROL
 *===========================================================================*/

/**
 * @brief Get current recursion depth
 * @param orch Orchestrator handle
 * @return Current depth
 */
uint32_t rcog_orchestrator_get_current_depth(
    const rcog_orchestrator_t* orch
);

/**
 * @brief Get maximum allowed depth
 * @param orch Orchestrator handle
 * @return Maximum depth
 */
uint32_t rcog_orchestrator_get_max_depth(
    const rcog_orchestrator_t* orch
);

/**
 * @brief Check if depth limit would be exceeded
 * @param orch Orchestrator handle
 * @param additional_depth Additional depth to check
 * @return true if limit would be exceeded
 */
bool rcog_orchestrator_would_exceed_depth(
    const rcog_orchestrator_t* orch,
    uint32_t additional_depth
);

/**
 * @brief Push recursion depth
 * @param orch Orchestrator handle
 * @return 0 on success, RCOG_ERROR_MAX_DEPTH_EXCEEDED if limit hit
 */
int rcog_orchestrator_push_depth(rcog_orchestrator_t* orch);

/**
 * @brief Pop recursion depth
 * @param orch Orchestrator handle
 */
void rcog_orchestrator_pop_depth(rcog_orchestrator_t* orch);

/*=============================================================================
 * IMMUNE MODULATION
 *===========================================================================*/

/**
 * @brief Apply immune modulation to orchestrator
 *
 * Adjusts orchestrator parameters based on immune system state:
 * - Reduces parallelism under inflammation
 * - Reduces max depth when sick
 * - Increases timeouts for slower processing
 *
 * @param orch Orchestrator handle
 * @param modulation Modulation parameters
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_apply_immune_modulation(
    rcog_orchestrator_t* orch,
    const rcog_immune_modulation_t* modulation
);

/**
 * @brief Get current effective limits after modulation
 * @param orch Orchestrator handle
 * @param max_depth Output maximum depth
 * @param max_parallel Output maximum parallelism
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_get_effective_limits(
    const rcog_orchestrator_t* orch,
    uint32_t* max_depth,
    uint32_t* max_parallel
);

/*=============================================================================
 * STRATEGY SELECTION
 *===========================================================================*/

/**
 * @brief Select optimal decomposition strategy
 *
 * Analyzes goal and context to select the best strategy.
 * May use imagination for simulation if enabled.
 *
 * @param orch Orchestrator handle
 * @param goal Goal to analyze
 * @param context Context store
 * @param strategy Output selected strategy
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_select_strategy(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    rcog_decomposition_strategy_t* strategy
);

/**
 * @brief Get strategy recommendation with confidence
 * @param orch Orchestrator handle
 * @param goal Goal to analyze
 * @param strategies Output array of strategies (ranked)
 * @param confidences Output confidence for each strategy
 * @param max_strategies Maximum strategies to return
 * @param num_strategies Output number of strategies
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_recommend_strategies(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    rcog_decomposition_strategy_t* strategies,
    float* confidences,
    size_t max_strategies,
    size_t* num_strategies
);

/*=============================================================================
 * SPAWNING SUB-ORCHESTRATORS
 *===========================================================================*/

/**
 * @brief Spawn a sub-orchestrator for nested decomposition
 *
 * Creates a child orchestrator with reduced depth limit.
 * Used for hierarchical decomposition.
 *
 * @param orch Parent orchestrator
 * @param parent_task Parent task context
 * @param sub_orch Output sub-orchestrator handle
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_spawn_sub(
    rcog_orchestrator_t* orch,
    const rcog_subtask_t* parent_task,
    rcog_orchestrator_t** sub_orch
);

/**
 * @brief Destroy sub-orchestrator
 * @param orch Parent orchestrator
 * @param sub_orch Sub-orchestrator to destroy
 */
void rcog_orchestrator_destroy_sub(
    rcog_orchestrator_t* orch,
    rcog_orchestrator_t* sub_orch
);

/*=============================================================================
 * STATISTICS AND TRACE
 *===========================================================================*/

/**
 * @brief Get orchestrator statistics
 * @param orch Orchestrator handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_get_stats(
    const rcog_orchestrator_t* orch,
    rcog_orchestrator_stats_t* stats
);

/**
 * @brief Reset orchestrator statistics
 * @param orch Orchestrator handle
 */
void rcog_orchestrator_reset_stats(rcog_orchestrator_t* orch);

/**
 * @brief Get execution trace
 * @param orch Orchestrator handle
 * @param trace Output trace (caller must free with rcog_orchestrator_free_trace)
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_get_trace(
    const rcog_orchestrator_t* orch,
    rcog_trace_t** trace
);

/**
 * @brief Free execution trace
 * @param trace Trace to free
 */
void rcog_orchestrator_free_trace(rcog_trace_t* trace);

/**
 * @brief Clear execution trace
 * @param orch Orchestrator handle
 */
void rcog_orchestrator_clear_trace(rcog_orchestrator_t* orch);

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

/**
 * @brief Validate decomposition for correctness
 *
 * Checks:
 * - No cycles in dependency graph
 * - All dependencies reference valid tasks
 * - Depth limits not exceeded
 *
 * @param decomp Decomposition to validate
 * @return 0 if valid, error code if invalid
 */
int rcog_orchestrator_validate_decomposition(
    const rcog_decomposition_t* decomp
);

/**
 * @brief Get topological order of subtasks
 *
 * Returns subtasks in order that respects dependencies.
 *
 * @param decomp Decomposition to order
 * @param order Output array of task IDs in execution order
 * @param max_order Maximum entries in order array
 * @param num_order Output number of entries
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_get_topological_order(
    const rcog_decomposition_t* decomp,
    uint64_t* order,
    size_t max_order,
    size_t* num_order
);

/**
 * @brief Get ready subtasks (no pending dependencies)
 * @param decomp Decomposition
 * @param completed Array of completed task IDs
 * @param num_completed Number of completed tasks
 * @param ready Output array of ready task IDs
 * @param max_ready Maximum entries
 * @param num_ready Output number of ready tasks
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_get_ready_subtasks(
    const rcog_decomposition_t* decomp,
    const uint64_t* completed,
    size_t num_completed,
    uint64_t* ready,
    size_t max_ready,
    size_t* num_ready
);

/**
 * @brief Estimate decomposition complexity
 * @param goal Goal to estimate
 * @param context Context store
 * @param complexity Output complexity estimate [0-1]
 * @return 0 on success, error code on failure
 */
int rcog_orchestrator_estimate_complexity(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const struct rcog_context_store* context,
    float* complexity
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_ORCHESTRATOR_H */
