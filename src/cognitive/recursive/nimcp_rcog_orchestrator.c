/**
 * @file nimcp_rcog_orchestrator.c
 * @brief Recursive Cognition Orchestrator Implementation
 * @version 1.0.0
 * @date 2026-01-03
 */

#include "cognitive/recursive/nimcp_rcog_orchestrator.h"
#include "cognitive/recursive/nimcp_rcog_context_store.h"
#include "cognitive/recursive/nimcp_rcog_answer.h"
#include "cognitive/recursive/nimcp_rcog_imagination_bridge.h"
#include "cognitive/recursive/nimcp_rcog_immune_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/algorithms/nimcp_sort.h"
#include "utils/algorithms/nimcp_monte_carlo.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <stdio.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(rcog_orchestrator, MESH_ADAPTER_CATEGORY_COGNITIVE)

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Active decomposition tracking
 */
typedef struct {
    uint64_t decomp_id;
    rcog_decomposition_t* decomp;
    bool in_use;
} rcog_active_decomp_t;

/**
 * @brief Batch handle for tracking dispatched subtasks
 */
struct rcog_batch_handle {
    uint64_t batch_id;               /**< Unique batch identifier */
    uint64_t* task_ids;              /**< Array of dispatched task IDs */
    size_t num_tasks;                /**< Number of tasks in batch */
    size_t capacity;                 /**< Capacity of task_ids array */
    rcog_subtask_status_t* statuses; /**< Status of each task */
    size_t completed_count;          /**< Number of completed tasks */
    bool all_done;                   /**< True when all tasks complete */
    uint64_t created_ms;             /**< Creation timestamp */
    nimcp_mutex_t* mutex;            /**< Synchronization mutex */
    nimcp_cond_t* cond;              /**< Completion condition variable */
};

/**
 * @brief Orchestrator internal structure
 */
struct rcog_orchestrator {
    /* Configuration */
    rcog_orchestrator_config_t config;

    /* Connections */
    struct rcog_context_store* context_store;
    struct rcog_answer_refiner* answer_refiner;
    struct rcog_delegation_pool* delegation_pool;
    struct rcog_imagination_bridge* imagination;
    struct rcog_immune_bridge* immune;
    struct rcog_engine* engine;

    /* Depth tracking */
    uint32_t current_depth;
    uint32_t effective_max_depth;
    uint32_t effective_max_parallel;

    /* Immune modulation */
    rcog_immune_modulation_t current_modulation;
    bool modulation_applied;

    /* Active decompositions */
    rcog_active_decomp_t active_decomps[RCOG_ORCH_MAX_ACTIVE_DECOMPOSITIONS];
    size_t num_active_decomps;
    uint64_t next_decomp_id;
    uint64_t next_task_id;

    /* Execution trace */
    rcog_trace_t* trace;
    bool trace_enabled;

    /* Statistics */
    rcog_orchestrator_stats_t stats;

    /* MCTS for strategy selection */
    uint32_t rand_seed;
    bool enable_mcts_strategy;
    uint32_t mcts_iterations;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/**
 * @brief Add trace entry
 */

/*=============================================================================
 * MCTS STRATEGY SELECTION
 *===========================================================================*/

/**
 * @brief State for MCTS strategy selection
 */
typedef struct {
    rcog_goal_type_t goal_type;
    uint32_t num_context_refs;
    uint32_t current_depth;
    uint32_t max_depth;
    rcog_decomposition_strategy_t selected_strategy;
    float estimated_confidence;
    float complexity;
} rcog_mcts_state_t;

/**
 * @brief User data for MCTS callbacks
 */
typedef struct {
    rcog_orchestrator_t* orch;
    const rcog_goal_t* goal;
} rcog_mcts_user_data_t;


/*=============================================================================
 * TOPOLOGICAL SORT CALLBACKS (for nimcp_sort.h API)
 *===========================================================================*/

/**
 * @brief Context for topological sort callbacks
 */
typedef struct {
    const rcog_decomposition_t* decomp;
    uint32_t* in_degrees;  /* Precomputed in-degrees */
} rcog_topo_context_t;

/**
 * @brief Get dependency count for a subtask (callback for nimcp_topological_sort)
 */


// Forward declarations for static functions (SRP split)
static void add_trace_entry( rcog_orchestrator_t* orch, uint64_t task_id, const char* event, float confidence );
static uint64_t generate_task_id(rcog_orchestrator_t* orch);
static uint64_t generate_decomp_id(rcog_orchestrator_t* orch);
static bool task_deps_satisfied( const rcog_decomposition_t* decomp, uint64_t task_id, const uint64_t* completed, size_t num_completed );
static bool detect_cycle_dfs( const rcog_dependency_graph_t* deps, const rcog_subtask_t* subtasks, size_t num_subtasks, uint64_t task_id, bool* visited, bool* in_stack );
static void apply_modulation_to_limits(rcog_orchestrator_t* orch);
static int decompose_sequential( rcog_orchestrator_t* orch, const rcog_goal_t* goal, const struct rcog_context_store* context, rcog_decomposition_t* result );
static int decompose_parallel( rcog_orchestrator_t* orch, const rcog_goal_t* goal, const struct rcog_context_store* context, rcog_decomposition_t* result );
static int decompose_hierarchical( rcog_orchestrator_t* orch, const rcog_goal_t* goal, const struct rcog_context_store* context, rcog_decomposition_t* result );
static int decompose_adaptive( rcog_orchestrator_t* orch, const rcog_goal_t* goal, const struct rcog_context_store* context, rcog_decomposition_t* result );
static uint32_t rcog_mcts_get_action_count(const void* state, void* user_data);
static uint32_t rcog_mcts_get_action(const void* state, uint32_t idx, void* user_data);
static void* rcog_mcts_apply_action(const void* state, uint32_t action, void* user_data);
static float rcog_mcts_evaluate(const void* state, void* user_data);
static bool rcog_mcts_is_terminal(const void* state, void* user_data);
static void rcog_mcts_free_state(void* state, void* user_data);
static void* rcog_mcts_clone_state(const void* state, void* user_data);
static rcog_decomposition_strategy_t select_strategy_mcts( rcog_orchestrator_t* orch, const rcog_goal_t* goal );
static uint32_t rcog_get_dep_count(uint32_t node_index, void* user_data);
static uint32_t rcog_get_dep(uint32_t node_index, uint32_t dep_index, void* user_data);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_rcog_orchestrator_part_accessors.c"  // 15 functions: accessors
#include "nimcp_rcog_orchestrator_part_core.c"  // 28 functions: core
#include "nimcp_rcog_orchestrator_part_helpers.c"  // 14 functions: helpers
#include "nimcp_rcog_orchestrator_part_lifecycle.c"  // 8 functions: lifecycle
