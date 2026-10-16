/**
 * @file nimcp_rcog_engine.h
 * @brief Recursive Cognition Engine - Main Processing Coordinator
 * @version 1.0.0
 * @date 2026-01-03
 *
 * WHAT: Central coordinator for recursive language model-style processing
 * WHY:  Provides unified interface to all recursive cognition components
 * HOW:  Orchestrates decomposition, delegation, tool routing, and answer refinement
 *
 * BIOLOGICAL BASIS:
 * The engine models the prefrontal cortex executive function:
 * - Goal-directed behavior coordination
 * - Working memory management
 * - Attention allocation across subsystems
 * - Performance monitoring and adjustment
 *
 * ARCHITECTURE:
 * ```
 * +------------------------------------------------------------------------------+
 * |                           RCOG ENGINE                                        |
 * |                                                                              |
 * |  +----------------+  +----------------+  +----------------+  +-------------+ |
 * |  | Context Store  |  | Orchestrator   |  | Delegation     |  | Tool        | |
 * |  | (variables)    |  | (decompose)    |  | Pool (execute) |  | Router      | |
 * |  +----------------+  +----------------+  +----------------+  +-------------+ |
 * |          |                   |                   |                |          |
 * |          +-------------------+-------------------+----------------+          |
 * |                              |                                               |
 * |  +----------------+  +----------------+  +----------------+  +-------------+ |
 * |  | Answer Refiner |  | Bio-Async      |  | Immune         |  | Imagination | |
 * |  | (aggregate)    |  | Bridge         |  | Bridge         |  | Bridge      | |
 * |  +----------------+  +----------------+  +----------------+  +-------------+ |
 * |                                                                              |
 * +------------------------------------------------------------------------------+
 *                                      |
 *                          +-----------+-----------+
 *                          |                       |
 *                    +-----v-----+           +-----v-----+
 *                    | Collective|           | Brain KG  |
 *                    | Bridge    |           | Bridge    |
 *                    +-----------+           +-----------+
 * ```
 *
 * KEY CONCEPTS:
 * - Goal Processing: Submit goals, get refined answers
 * - Context Management: Environment-as-variable pattern (RLM)
 * - Recursive Decomposition: Break goals into parallelizable subtasks
 * - Answer Diffusion: Iterative refinement until confidence threshold
 * - System Integration: Bio-async, immune, imagination bridges
 */

#ifndef NIMCP_RCOG_ENGINE_H
#define NIMCP_RCOG_ENGINE_H

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
struct rcog_orchestrator;
struct rcog_delegation_pool;
struct rcog_answer_refiner;
struct rcog_tool_router;
struct rcog_bio_async_bridge;
struct rcog_immune_bridge;
struct rcog_imagination_bridge;
struct rcog_collective_bridge;
struct rcog_brain_kg_bridge;

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

/** Maximum concurrent goal processing */
#define RCOG_ENGINE_MAX_CONCURRENT_GOALS 16

/** Default processing timeout (ms) */
#define RCOG_ENGINE_DEFAULT_TIMEOUT_MS 60000

/** Maximum answer content size */
#define RCOG_ENGINE_MAX_ANSWER_SIZE (10 * 1024 * 1024)

/** Default confidence threshold */
#define RCOG_ENGINE_DEFAULT_CONFIDENCE_THRESHOLD 0.95f

/*=============================================================================
 * ENGINE STATE
 *===========================================================================*/

/**
 * @brief Engine operational state
 */
typedef enum {
    RCOG_ENGINE_UNINITIALIZED = 0,
    RCOG_ENGINE_INITIALIZING,
    RCOG_ENGINE_READY,
    RCOG_ENGINE_PROCESSING,
    RCOG_ENGINE_PAUSED,
    RCOG_ENGINE_DEGRADED,       /**< Reduced capacity (immune modulation) */
    RCOG_ENGINE_SHUTTING_DOWN,
    RCOG_ENGINE_STOPPED
} rcog_engine_state_t;

/**
 * @brief Processing mode
 */
typedef enum {
    RCOG_MODE_SYNC = 0,         /**< Blocking until answer ready */
    RCOG_MODE_ASYNC,            /**< Non-blocking with callback */
    RCOG_MODE_STREAMING         /**< Progress updates during processing */
} rcog_processing_mode_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Engine configuration
 */
typedef struct {
    /* Processing limits */
    uint32_t max_recursion_depth;
    uint32_t max_parallel_subtasks;
    uint32_t max_concurrent_goals;
    uint32_t default_timeout_ms;

    /* Answer thresholds */
    float confidence_threshold;
    uint32_t max_refinement_steps;
    bool enable_early_termination;

    /* Decomposition */
    rcog_decomposition_strategy_t default_strategy;
    bool enable_adaptive_strategy;

    /* Integration */
    bool enable_bio_async;
    bool enable_immune_modulation;
    bool enable_imagination;
    bool enable_collective;
    bool enable_brain_kg;

    /* Builtin tools */
    bool register_l1_builtins;   /**< Reasoning tools */
    bool register_l2_builtins;   /**< Perception tools */
    bool register_l3_builtins;   /**< Action tools */

    /* Debug */
    bool enable_tracing;
    bool verbose_logging;
} rcog_engine_config_t;

/*=============================================================================
 * PROCESSING REQUEST/RESULT
 *===========================================================================*/

/**
 * @brief Goal processing request
 */
typedef struct {
    rcog_goal_t goal;            /**< Goal to process */
    rcog_processing_mode_t mode; /**< Processing mode */
    uint32_t timeout_ms;         /**< Override timeout (0 = use default) */

    /* Callbacks (for async/streaming) */
    rcog_progress_callback_t progress_callback;
    void* progress_user_data;

    /* Options */
    bool skip_decomposition;     /**< Process as single unit */
    bool force_local;            /**< Don't distribute to collective */
    rcog_decomposition_strategy_t strategy_override; /**< Force specific strategy */
} rcog_process_request_t;

/**
 * @brief Goal processing result
 */
typedef struct {
    uint64_t request_id;         /**< Request identifier */
    rcog_goal_t goal;            /**< Original goal */
    rcog_answer_state_t answer;  /**< Final answer state */

    /* Metadata */
    bool success;
    rcog_error_t error;
    const char* error_message;

    /* Statistics */
    uint32_t subtasks_created;
    uint32_t subtasks_completed;
    uint32_t subtasks_failed;
    uint32_t max_depth_used;
    uint32_t refinement_steps;
    uint64_t processing_time_ms;
} rcog_process_result_t;

/**
 * @brief Async processing handle
 */
typedef struct rcog_request_handle {
    uint64_t request_id;
    rcog_goal_t goal;
    rcog_processing_mode_t mode;
    rcog_engine_state_t state;
    bool completed;
    bool cancelled;
    rcog_process_result_t result;
} rcog_request_handle_t;

/*=============================================================================
 * ENGINE STATISTICS
 *===========================================================================*/

/**
 * @brief Engine statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t goals_submitted;
    uint64_t goals_completed;
    uint64_t goals_failed;
    uint64_t goals_cancelled;
    uint64_t goals_timeout;

    /* Subtask counts */
    uint64_t subtasks_created;
    uint64_t subtasks_completed;
    uint64_t subtasks_failed;

    /* Answer stats */
    float avg_confidence;
    float avg_refinement_steps;
    uint32_t max_depth_reached;

    /* Timing */
    float avg_processing_time_ms;
    uint64_t total_processing_time_ms;

    /* Current state */
    uint32_t active_goals;
    uint32_t pending_goals;
    rcog_engine_state_t state;

    /* Resource usage */
    size_t memory_usage;
    size_t peak_memory_usage;

    /* Integration stats */
    uint64_t bio_async_messages;
    uint64_t immune_modulations;
    uint64_t imagination_simulations;
    uint64_t collective_distributions;
    uint64_t kg_queries;
} rcog_engine_stats_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Get default engine configuration
 * @return Default configuration
 */
rcog_engine_config_t rcog_engine_default_config(void);

/**
 * @brief Create engine with configuration
 * @param config Configuration (NULL for defaults)
 * @return Engine handle or NULL on error
 */
rcog_engine_t* rcog_engine_create(const rcog_engine_config_t* config);

/**
 * @brief Create engine with default configuration
 * @return Engine handle or NULL on error
 */
rcog_engine_t* rcog_engine_create_default(void);

/**
 * @brief Initialize engine (create all subsystems)
 * @param engine Engine handle
 * @return 0 on success, error code on failure
 */
int rcog_engine_init(rcog_engine_t* engine);

/**
 * @brief Start engine (begin accepting goals)
 * @param engine Engine handle
 * @return 0 on success, error code on failure
 */
int rcog_engine_start(rcog_engine_t* engine);

/**
 * @brief Stop engine (finish pending, reject new)
 * @param engine Engine handle
 * @param timeout_ms Wait timeout for pending goals
 * @return 0 on success, error code on failure
 */
int rcog_engine_stop(rcog_engine_t* engine, uint32_t timeout_ms);

/**
 * @brief Destroy engine and free resources
 * @param engine Engine handle (NULL safe)
 */
void rcog_engine_destroy(rcog_engine_t* engine);

/**
 * @brief Get engine state
 * @param engine Engine handle
 * @return Current state
 */
rcog_engine_state_t rcog_engine_get_state(const rcog_engine_t* engine);

/*=============================================================================
 * SUBSYSTEM ACCESS
 *===========================================================================*/

/**
 * @brief Get context store
 * @param engine Engine handle
 * @return Context store or NULL
 */
struct rcog_context_store* rcog_engine_get_context_store(rcog_engine_t* engine);

/**
 * @brief Get orchestrator
 * @param engine Engine handle
 * @return Orchestrator or NULL
 */
struct rcog_orchestrator* rcog_engine_get_orchestrator(rcog_engine_t* engine);

/**
 * @brief Get delegation pool
 * @param engine Engine handle
 * @return Delegation pool or NULL
 */
struct rcog_delegation_pool* rcog_engine_get_delegation_pool(rcog_engine_t* engine);

/**
 * @brief Get tool router
 * @param engine Engine handle
 * @return Tool router or NULL
 */
struct rcog_tool_router* rcog_engine_get_tool_router(rcog_engine_t* engine);

/**
 * @brief Get answer refiner
 * @param engine Engine handle
 * @return Answer refiner or NULL
 */
struct rcog_answer_refiner* rcog_engine_get_answer_refiner(rcog_engine_t* engine);

/*=============================================================================
 * BRIDGE CONNECTION
 *===========================================================================*/

/**
 * @brief Connect bio-async bridge
 * @param engine Engine handle
 * @param bridge Bio-async bridge
 * @return 0 on success, error code on failure
 */
int rcog_engine_connect_bio_async(
    rcog_engine_t* engine,
    struct rcog_bio_async_bridge* bridge
);

/**
 * @brief Connect immune bridge
 * @param engine Engine handle
 * @param bridge Immune bridge
 * @return 0 on success, error code on failure
 */
int rcog_engine_connect_immune(
    rcog_engine_t* engine,
    struct rcog_immune_bridge* bridge
);

/**
 * @brief Connect imagination bridge
 * @param engine Engine handle
 * @param bridge Imagination bridge
 * @return 0 on success, error code on failure
 */
int rcog_engine_connect_imagination(
    rcog_engine_t* engine,
    struct rcog_imagination_bridge* bridge
);

/**
 * @brief Connect collective bridge
 * @param engine Engine handle
 * @param bridge Collective bridge
 * @return 0 on success, error code on failure
 */
int rcog_engine_connect_collective(
    rcog_engine_t* engine,
    struct rcog_collective_bridge* bridge
);

/**
 * @brief Connect brain knowledge graph bridge
 * @param engine Engine handle
 * @param bridge Brain KG bridge
 * @return 0 on success, error code on failure
 */
int rcog_engine_connect_brain_kg(
    rcog_engine_t* engine,
    struct rcog_brain_kg_bridge* bridge
);

/*=============================================================================
 * GOAL PROCESSING
 *===========================================================================*/

/**
 * @brief Process goal synchronously
 *
 * Blocks until answer is ready or timeout.
 *
 * @param engine Engine handle
 * @param goal Goal to process
 * @param result Output result
 * @return 0 on success, error code on failure
 */
int rcog_engine_process(
    rcog_engine_t* engine,
    const rcog_goal_t* goal,
    rcog_process_result_t* result
);

/**
 * @brief Process goal with full options
 * @param engine Engine handle
 * @param request Processing request
 * @param result Output result (for sync mode)
 * @param handle Output handle (for async mode)
 * @return 0 on success, error code on failure
 */
int rcog_engine_process_ex(
    rcog_engine_t* engine,
    const rcog_process_request_t* request,
    rcog_process_result_t* result,
    rcog_request_handle_t** handle
);

/**
 * @brief Process goal asynchronously
 * @param engine Engine handle
 * @param goal Goal to process
 * @param callback Completion callback
 * @param user_data Callback user data
 * @param handle Output request handle
 * @return 0 on success, error code on failure
 */
int rcog_engine_process_async(
    rcog_engine_t* engine,
    const rcog_goal_t* goal,
    rcog_progress_callback_t callback,
    void* user_data,
    rcog_request_handle_t** handle
);

/**
 * @brief Wait for async request completion
 * @param engine Engine handle
 * @param handle Request handle
 * @param timeout_ms Timeout (0 = infinite)
 * @param result Output result
 * @return 0 on success, error code on failure/timeout
 */
int rcog_engine_await(
    rcog_engine_t* engine,
    rcog_request_handle_t* handle,
    uint32_t timeout_ms,
    rcog_process_result_t* result
);

/**
 * @brief Cancel pending request
 * @param engine Engine handle
 * @param handle Request handle
 * @return 0 on success, error code on failure
 */
int rcog_engine_cancel(
    rcog_engine_t* engine,
    rcog_request_handle_t* handle
);

/**
 * @brief Free request handle
 * @param handle Handle to free
 */
void rcog_engine_free_handle(rcog_request_handle_t* handle);

/**
 * @brief Free processing result
 * @param result Result to free
 */
void rcog_engine_free_result(rcog_process_result_t* result);

/*=============================================================================
 * CONTEXT MANAGEMENT
 *===========================================================================*/

/**
 * @brief Set context variable
 * @param engine Engine handle
 * @param name Variable name
 * @param data Variable data
 * @param size Data size
 * @param dtype Data type
 * @return 0 on success, error code on failure
 */
int rcog_engine_set_context(
    rcog_engine_t* engine,
    const char* name,
    const void* data,
    size_t size,
    rcog_data_type_t dtype
);

/**
 * @brief Get context variable
 * @param engine Engine handle
 * @param name Variable name
 * @param result Output query result
 * @return 0 on success, error code on failure
 */
int rcog_engine_get_context(
    rcog_engine_t* engine,
    const char* name,
    rcog_query_result_t* result
);

/**
 * @brief Query context with parameters
 * @param engine Engine handle
 * @param name Variable name
 * @param pattern Access pattern
 * @param params Query parameters
 * @param result Output query result
 * @return 0 on success, error code on failure
 */
int rcog_engine_query_context(
    rcog_engine_t* engine,
    const char* name,
    rcog_access_pattern_t pattern,
    const rcog_query_params_t* params,
    rcog_query_result_t* result
);

/**
 * @brief Clear context variable
 * @param engine Engine handle
 * @param name Variable name
 * @return 0 on success, error code on failure
 */
int rcog_engine_clear_context(
    rcog_engine_t* engine,
    const char* name
);

/**
 * @brief Clear all context
 * @param engine Engine handle
 * @return 0 on success, error code on failure
 */
int rcog_engine_clear_all_context(rcog_engine_t* engine);

/*=============================================================================
 * TOOL MANAGEMENT
 *===========================================================================*/

/**
 * @brief Register custom tool
 * @param engine Engine handle
 * @param name Tool name
 * @param handler Tool handler function
 * @param tier Minimum capability tier
 * @param context Tool context
 * @return 0 on success, error code on failure
 */
int rcog_engine_register_tool(
    rcog_engine_t* engine,
    const char* name,
    rcog_tool_fn handler,
    rcog_capability_tier_t tier,
    void* context
);

/**
 * @brief Unregister tool
 * @param engine Engine handle
 * @param name Tool name
 * @return 0 on success, error code on failure
 */
int rcog_engine_unregister_tool(
    rcog_engine_t* engine,
    const char* name
);

/**
 * @brief List available tools for tier
 * @param engine Engine handle
 * @param tier Capability tier
 * @param tools Output array of tool names
 * @param max_tools Maximum tools
 * @param num_tools Output number of tools
 * @return 0 on success, error code on failure
 */
int rcog_engine_list_tools(
    rcog_engine_t* engine,
    rcog_capability_tier_t tier,
    char (*tools)[64],
    size_t max_tools,
    size_t* num_tools
);

/*=============================================================================
 * IMMUNE MODULATION
 *===========================================================================*/

/**
 * @brief Apply immune modulation
 * @param engine Engine handle
 * @param modulation Modulation parameters
 * @return 0 on success, error code on failure
 */
int rcog_engine_apply_immune_modulation(
    rcog_engine_t* engine,
    const rcog_immune_modulation_t* modulation
);

/**
 * @brief Get current immune modulation
 * @param engine Engine handle
 * @param modulation Output modulation
 * @return 0 on success, error code on failure
 */
int rcog_engine_get_immune_modulation(
    const rcog_engine_t* engine,
    rcog_immune_modulation_t* modulation
);

/**
 * @brief Enter degraded mode
 * @param engine Engine handle
 * @return 0 on success, error code on failure
 */
int rcog_engine_enter_degraded_mode(rcog_engine_t* engine);

/**
 * @brief Exit degraded mode
 * @param engine Engine handle
 * @return 0 on success, error code on failure
 */
int rcog_engine_exit_degraded_mode(rcog_engine_t* engine);

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Get engine statistics
 * @param engine Engine handle
 * @param stats Output statistics
 * @return 0 on success, error code on failure
 */
int rcog_engine_get_stats(
    const rcog_engine_t* engine,
    rcog_engine_stats_t* stats
);

/**
 * @brief Reset engine statistics
 * @param engine Engine handle
 */
void rcog_engine_reset_stats(rcog_engine_t* engine);

/**
 * @brief Get processing progress
 * @param engine Engine handle
 * @param handle Request handle (NULL for overall)
 * @param progress Output progress
 * @return 0 on success, error code on failure
 */
int rcog_engine_get_progress(
    const rcog_engine_t* engine,
    const rcog_request_handle_t* handle,
    rcog_progress_t* progress
);

/*=============================================================================
 * UTILITY
 *===========================================================================*/

/**
 * @brief Create default processing request
 * @param goal Goal to process
 * @return Default request
 */
rcog_process_request_t rcog_engine_default_request(const rcog_goal_t* goal);

/**
 * @brief Create goal from query string
 * @param query Query string
 * @param type Goal type
 * @return Goal structure
 */
rcog_goal_t rcog_engine_create_goal(
    const char* query,
    rcog_goal_type_t type
);

/**
 * @brief Get engine state name
 * @param state Engine state
 * @return State name string
 */
const char* rcog_engine_state_name(rcog_engine_state_t state);

/**
 * @brief Check if engine is ready to process
 * @param engine Engine handle
 * @return true if ready
 */
bool rcog_engine_is_ready(const rcog_engine_t* engine);

/**
 * @brief Check if engine has capacity for more goals
 * @param engine Engine handle
 * @return true if has capacity
 */
bool rcog_engine_has_capacity(const rcog_engine_t* engine);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_ENGINE_H */
