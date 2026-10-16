/**
 * @file nimcp_rcog_types.h
 * @brief Core type definitions for Recursive Cognition module
 *
 * WHAT: Defines all fundamental types, enums, and constants for recursive cognition
 * WHY:  Centralized type definitions enable consistent API across all components
 * HOW:  Shared header included by all recursive cognition submodules
 *
 * INSPIRED BY: Prime Intellect Recursive Language Models (RLMs)
 * - Context folding instead of summarization
 * - Environment-based input (never directly in processing window)
 * - Hierarchical delegation with capability tiers
 * - Answer diffusion (iterative refinement)
 *
 * @author NIMCP Development Team
 * @date 2026-01-03
 * @version 1.0.0
 */

#ifndef NIMCP_RCOG_TYPES_H
#define NIMCP_RCOG_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Error Codes
//=============================================================================

/* Include canonical error type definition */
#ifndef NIMCP_ERROR_TYPE_DEFINED
#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_memory.h"
#endif

/**
 * @brief Recursive cognition error codes (0x1400xx range)
 */
typedef enum {
    RCOG_OK = 0,
    RCOG_ERROR_NULL_POINTER         = 0x140001,
    RCOG_ERROR_INVALID_CONFIG       = 0x140002,
    RCOG_ERROR_CONTEXT_NOT_FOUND    = 0x140003,
    RCOG_ERROR_CONTEXT_TOO_LARGE    = 0x140004,
    RCOG_ERROR_CONTEXT_FULL         = 0x140005,
    RCOG_ERROR_MAX_DEPTH_EXCEEDED   = 0x140006,
    RCOG_ERROR_TIMEOUT              = 0x140007,
    RCOG_ERROR_SUBTASK_FAILED       = 0x140008,
    RCOG_ERROR_TOOL_ACCESS_DENIED   = 0x140009,
    RCOG_ERROR_TOOL_NOT_FOUND       = 0x14000A,
    RCOG_ERROR_ANSWER_NOT_READY     = 0x14000B,
    RCOG_ERROR_BIO_ASYNC_DISCONNECTED = 0x14000C,
    RCOG_ERROR_OUT_OF_MEMORY        = 0x14000D,
    RCOG_ERROR_WORKER_POOL_EXHAUSTED = 0x14000E,
    RCOG_ERROR_INVALID_QUERY        = 0x14000F,
    RCOG_ERROR_COMPRESSION_FAILED   = 0x140010,
    RCOG_ERROR_DECOMPRESSION_FAILED = 0x140011,
    RCOG_ERROR_PATTERN_QUARANTINED  = 0x140012,
    RCOG_ERROR_IMMUNE_SHUTDOWN      = 0x140013,
    RCOG_ERROR_SWARM_DISCONNECTED   = 0x140014,
    RCOG_ERROR_IMAGINATION_FAILED   = 0x140015,
    RCOG_ERROR_KG_NOT_CONNECTED     = 0x140016,
    RCOG_ERROR_ALREADY_INITIALIZED  = 0x140017,
    RCOG_ERROR_NOT_INITIALIZED      = 0x140018
} rcog_error_t;

//=============================================================================
// Configuration Constants
//=============================================================================

/**
 * @brief Default maximum recursion depth
 */
#define RCOG_DEFAULT_MAX_DEPTH 16

/**
 * @brief Default maximum parallel subtasks
 */
#define RCOG_DEFAULT_MAX_PARALLEL_SUBTASKS 8

/**
 * @brief Default answer ready threshold (confidence)
 */
#define RCOG_DEFAULT_READY_THRESHOLD 0.95f

/**
 * @brief Default maximum refinement iterations
 */
#define RCOG_DEFAULT_MAX_REFINEMENT_STEPS 32

/**
 * @brief Default context store output limit per query (characters)
 */
#define RCOG_DEFAULT_OUTPUT_LIMIT 8192

/**
 * @brief Maximum named variables in context store
 */
#define RCOG_MAX_VARIABLES 64

/**
 * @brief Maximum size per variable in bytes (1 MB)
 */
#define RCOG_MAX_VARIABLE_SIZE (1024 * 1024)

/**
 * @brief Maximum variable name length
 */
#define RCOG_MAX_VARIABLE_NAME_LEN 64

/**
 * @brief Default latent dimension for JEPA integration
 */
#define RCOG_DEFAULT_LATENT_DIM 256

/**
 * @brief Maximum latent dimension
 */
#define RCOG_MAX_LATENT_DIM 512

/**
 * @brief Default learning rate for answer refinement
 */
#define RCOG_DEFAULT_LEARNING_RATE 0.1f

/**
 * @brief Default momentum for answer refinement
 */
#define RCOG_DEFAULT_MOMENTUM 0.9f

/**
 * @brief Convergence epsilon for early stopping
 */
#define RCOG_DEFAULT_CONVERGENCE_EPSILON 0.001f

//=============================================================================
// Core Enumerations
//=============================================================================

/**
 * @brief Data types for context variables
 */
typedef enum {
    RCOG_DTYPE_UNKNOWN = 0,
    RCOG_DTYPE_TEXT,            /**< UTF-8 text (char*) */
    RCOG_DTYPE_BINARY,          /**< Raw binary data */
    RCOG_DTYPE_TENSOR,          /**< NIMCP tensor */
    RCOG_DTYPE_JSON,            /**< JSON-formatted text */
    RCOG_DTYPE_GRAPH,           /**< Knowledge graph fragment */
    RCOG_DTYPE_EMBEDDING,       /**< Vector embedding */
    RCOG_DTYPE_COUNT
} rcog_data_type_t;

/**
 * @brief Context variable access patterns (mirrors RLM REPL patterns)
 */
typedef enum {
    RCOG_ACCESS_FULL = 0,       /**< Return entire variable (up to output limit) */
    RCOG_ACCESS_SLICE,          /**< Get range: var[start:end] */
    RCOG_ACCESS_SEARCH,         /**< Search for pattern in variable */
    RCOG_ACCESS_HEAD,           /**< First N items/characters */
    RCOG_ACCESS_TAIL,           /**< Last N items/characters */
    RCOG_ACCESS_SAMPLE,         /**< Random sample of N items */
    RCOG_ACCESS_AGGREGATE,      /**< Apply aggregation function */
    RCOG_ACCESS_METADATA        /**< Return variable metadata only */
} rcog_access_pattern_t;

/**
 * @brief Capability tiers for tool separation (RLM pattern)
 *
 * Higher tiers have access to more tools but are further from root.
 * Root coordinator has NO tools - only coordination primitives.
 */
typedef enum {
    RCOG_TIER_ROOT = 0,         /**< Coordination only (no tools) */
    RCOG_TIER_L1_REASONING,     /**< Memory access, logic, planning */
    RCOG_TIER_L2_PERCEPTION,    /**< Sensory processing, feature extraction */
    RCOG_TIER_L3_ACTION,        /**< Motor control, output generation */
    RCOG_TIER_L4_SPECIALIZED,   /**< Domain-specific tools (vision, audio, etc.) */
    RCOG_TIER_COUNT
} rcog_capability_tier_t;

/**
 * @brief Goal types for recursive processing
 */
typedef enum {
    RCOG_GOAL_QUESTION_ANSWERING = 0,   /**< Answer a question from context */
    RCOG_GOAL_SUMMARIZATION,            /**< Summarize context */
    RCOG_GOAL_EXTRACTION,               /**< Extract specific information */
    RCOG_GOAL_REASONING,                /**< Multi-step logical reasoning */
    RCOG_GOAL_PLANNING,                 /**< Plan a sequence of actions */
    RCOG_GOAL_ANALYSIS,                 /**< Analyze and compare */
    RCOG_GOAL_GENERATION,               /**< Generate new content */
    RCOG_GOAL_TRANSLATION,              /**< Transform between formats */
    RCOG_GOAL_VALIDATION,               /**< Validate against rules */
    RCOG_GOAL_CUSTOM                    /**< User-defined goal type */
} rcog_goal_type_t;

/**
 * @brief Subtask status
 */
typedef enum {
    RCOG_SUBTASK_PENDING = 0,
    RCOG_SUBTASK_QUEUED,
    RCOG_SUBTASK_RUNNING,
    RCOG_SUBTASK_COMPLETED,
    RCOG_SUBTASK_FAILED,
    RCOG_SUBTASK_CANCELLED,
    RCOG_SUBTASK_TIMEOUT
} rcog_subtask_status_t;

/**
 * @brief Answer refinement state
 */
typedef enum {
    RCOG_ANSWER_INITIALIZING = 0,
    RCOG_ANSWER_REFINING,
    RCOG_ANSWER_CONVERGING,
    RCOG_ANSWER_READY,
    RCOG_ANSWER_STALLED,        /**< Not converging */
    RCOG_ANSWER_FAILED
} rcog_answer_status_t;

/**
 * @brief Decomposition strategies
 */
typedef enum {
    RCOG_DECOMP_SEQUENTIAL = 0, /**< Process subtasks in order */
    RCOG_DECOMP_PARALLEL,       /**< Process all subtasks simultaneously */
    RCOG_DECOMP_HIERARCHICAL,   /**< Recursive tree decomposition */
    RCOG_DECOMP_ADAPTIVE,       /**< Choose strategy based on goal/context */
    RCOG_DECOMP_SWARM           /**< Distribute across collective */
} rcog_decomposition_strategy_t;

/**
 * @brief Scheduling policies for delegation pool
 */
typedef enum {
    RCOG_SCHED_PRIORITY = 0,    /**< Higher priority first */
    RCOG_SCHED_ROUND_ROBIN,     /**< Fair round-robin */
    RCOG_SCHED_ADAPTIVE,        /**< Adapt based on load/complexity */
    RCOG_SCHED_WORK_STEALING    /**< Work stealing for load balance */
} rcog_scheduling_policy_t;

/**
 * @brief State transitions that trigger glial waves
 */
typedef enum {
    RCOG_TRANSITION_START_PROCESSING = 0,
    RCOG_TRANSITION_DEPTH_LIMIT_REACHED,
    RCOG_TRANSITION_ANSWER_READY,
    RCOG_TRANSITION_EMERGENCY_STOP,
    RCOG_TRANSITION_SWARM_HANDOFF,
    RCOG_TRANSITION_IMMUNE_DEGRADED,
    RCOG_TRANSITION_RESOURCE_CRITICAL
} rcog_state_transition_t;

//=============================================================================
// Forward Declarations (Opaque Handles)
//=============================================================================

/** @brief Context store opaque handle */
typedef struct rcog_context_store rcog_context_store_t;

/** @brief Orchestrator opaque handle */
typedef struct rcog_orchestrator rcog_orchestrator_t;

/** @brief Delegation pool opaque handle */
typedef struct rcog_delegation_pool rcog_delegation_pool_t;

/** @brief Answer refiner opaque handle */
typedef struct rcog_answer_refiner rcog_answer_refiner_t;

/** @brief Tool router opaque handle */
typedef struct rcog_tool_router rcog_tool_router_t;

/** @brief Recursive cognition engine opaque handle */
typedef struct rcog_engine rcog_engine_t;

/** @brief Batch handle for parallel subtasks */
typedef struct rcog_batch_handle rcog_batch_handle_t;

/** @brief Collective handle for swarm delegation */
typedef struct rcog_collective_handle rcog_collective_handle_t;

/** @brief Async processing handle */
typedef struct rcog_async_handle rcog_async_handle_t;

//=============================================================================
// Core Structures
//=============================================================================

/**
 * @brief Query parameters for context access
 */
typedef struct {
    size_t start;               /**< Start index/offset for SLICE */
    size_t end;                 /**< End index/offset for SLICE */
    size_t count;               /**< Number of items for HEAD/TAIL/SAMPLE */
    const char* pattern;        /**< Search pattern for SEARCH */
    const char* aggregator;     /**< Aggregation function name */
    size_t output_limit;        /**< Max output size (0 = default) */
} rcog_query_params_t;

/**
 * @brief Query result from context store
 */
typedef struct {
    void* data;                 /**< Result data (caller must free if owns=true) */
    size_t size;                /**< Size of result data */
    rcog_data_type_t dtype;     /**< Data type of result */
    bool owns_data;             /**< True if caller must free data */
    bool truncated;             /**< True if output was truncated */
    bool found;                 /**< True if variable/pattern was found */
    size_t total_size;          /**< Total size before truncation */
    size_t match_count;         /**< Number of matches (for SEARCH) */
} rcog_query_result_t;

/**
 * @brief Variable metadata
 */
typedef struct {
    char name[RCOG_MAX_VARIABLE_NAME_LEN];  /**< Variable name */
    rcog_data_type_t dtype;     /**< Data type */
    size_t size;                /**< Size in bytes */
    size_t item_count;          /**< Number of items (for arrays/text) */
    uint64_t created_ms;        /**< Creation timestamp */
    uint64_t accessed_ms;       /**< Last access timestamp */
    uint32_t access_count;      /**< Number of accesses */
    bool compressed;            /**< True if stored compressed */
    bool shared_with_swarm;     /**< True if shared via collective */
    float salience;             /**< Salience for swarm sharing */
} rcog_variable_metadata_t;

/**
 * @brief Goal specification
 */
typedef struct {
    rcog_goal_type_t type;      /**< Goal type */
    const char* query;          /**< Natural language query/instruction */
    const char** context_refs;  /**< Array of context variable names */
    size_t num_context_refs;    /**< Number of context references */
    float priority;             /**< Priority (0-1, higher = more urgent) */
    uint32_t timeout_ms;        /**< Timeout for this goal (0 = default) */
    uint32_t max_depth;         /**< Max recursion depth (0 = default) */
    void* user_data;            /**< User-defined context */
} rcog_goal_t;

/**
 * @brief Subtask specification
 */
typedef struct rcog_subtask {
    uint64_t task_id;           /**< Unique task identifier */
    rcog_goal_t goal;           /**< Subtask goal */
    rcog_capability_tier_t tier; /**< Required capability tier */
    rcog_subtask_status_t status; /**< Current status */
    float priority;             /**< Scheduling priority */
    uint32_t timeout_ms;        /**< Timeout for this subtask */
    uint32_t depth;             /**< Current recursion depth */
    struct rcog_subtask* parent; /**< Parent task (for depth tracking) */
    uint64_t started_ms;        /**< Start timestamp */
    uint64_t completed_ms;      /**< Completion timestamp */
} rcog_subtask_t;

/**
 * @brief Subtask result
 */
typedef struct {
    uint64_t task_id;           /**< Task identifier */
    rcog_subtask_status_t status; /**< Final status */
    bool success;               /**< True if completed successfully */
    void* result_data;          /**< Result data */
    size_t result_size;         /**< Size of result */
    rcog_data_type_t result_dtype; /**< Type of result */
    float confidence;           /**< Confidence in result (decays) */
    float* latent;              /**< Latent representation (for answer refinement) */
    size_t latent_dim;          /**< Dimension of latent representation */
    rcog_error_t error;         /**< Error code if failed */
    const char* error_message;  /**< Error message if failed */
    uint64_t duration_ms;       /**< Processing duration */
} rcog_subtask_result_t;

/**
 * @brief Answer state (RLM's {content, ready} pattern)
 */
typedef struct {
    void* content;              /**< Current answer representation */
    size_t content_size;        /**< Size of content */
    float* latent;              /**< JEPA latent representation */
    size_t latent_dim;          /**< Latent dimension */
    float confidence;           /**< Current confidence level [0,1] */
    bool ready;                 /**< True when ready for output */
    rcog_answer_status_t status; /**< Current status */
    uint32_t refinement_step;   /**< Current iteration number */
    float delta;                /**< Change from last step */
    uint64_t started_ms;        /**< Processing start time */
    uint64_t last_updated_ms;   /**< Last refinement time */
    bool heap_allocated;        /**< True if allocated by rcog_answer_state_create */
} rcog_answer_state_t;

/**
 * @brief Progress information for streaming callbacks
 */
typedef struct {
    size_t completed_subtasks;
    size_t total_subtasks;
    size_t active_subtasks;
    uint32_t current_depth;
    uint32_t max_depth_reached;
    float current_confidence;
    uint32_t refinement_step;
    uint64_t elapsed_ms;
} rcog_progress_t;

/**
 * @brief Immune modulation effects on recursive cognition
 */
typedef struct {
    float capacity_multiplier;      /**< 0.0-1.0, reduces under inflammation */
    float max_depth_multiplier;     /**< Reduce recursion depth when sick */
    float parallelism_multiplier;   /**< Reduce parallel subtasks */
    float timeout_multiplier;       /**< Increase timeouts (slower processing) */
    bool enable_degraded_mode;      /**< Simplify decomposition strategy */
} rcog_immune_modulation_t;

/**
 * @brief Processing statistics
 */
typedef struct {
    uint64_t total_goals_processed;
    uint64_t total_subtasks_created;
    uint64_t total_subtasks_completed;
    uint64_t total_subtasks_failed;
    uint64_t total_context_queries;
    uint64_t total_refinement_steps;
    uint64_t max_depth_reached;
    float avg_confidence;
    float avg_refinement_steps;
    uint64_t total_processing_time_ms;
    size_t current_memory_usage;
    size_t peak_memory_usage;
} rcog_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Progress callback for streaming processing
 */
typedef void (*rcog_progress_callback_t)(
    const rcog_progress_t* progress,
    void* user_data
);

/**
 * @brief Subtask completion callback
 */
typedef void (*rcog_subtask_callback_t)(
    const rcog_subtask_result_t* result,
    void* user_data
);

/**
 * @brief Context helper function (like RLM Python helpers)
 */
typedef rcog_error_t (*rcog_helper_fn)(
    const void* variable_data,
    size_t variable_size,
    rcog_data_type_t dtype,
    void* helper_context,
    rcog_query_result_t* result
);

/**
 * @brief Tool execution function
 */
typedef rcog_error_t (*rcog_tool_fn)(
    const void* input,
    size_t input_size,
    void* tool_context,
    void** output,
    size_t* output_size
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error code name as string
 */
static inline const char* rcog_error_name(rcog_error_t error) {
    switch (error) {
        case RCOG_OK: return "OK";
        case RCOG_ERROR_NULL_POINTER: return "NULL_POINTER";
        case RCOG_ERROR_INVALID_CONFIG: return "INVALID_CONFIG";
        case RCOG_ERROR_CONTEXT_NOT_FOUND: return "CONTEXT_NOT_FOUND";
        case RCOG_ERROR_CONTEXT_TOO_LARGE: return "CONTEXT_TOO_LARGE";
        case RCOG_ERROR_CONTEXT_FULL: return "CONTEXT_FULL";
        case RCOG_ERROR_MAX_DEPTH_EXCEEDED: return "MAX_DEPTH_EXCEEDED";
        case RCOG_ERROR_TIMEOUT: return "TIMEOUT";
        case RCOG_ERROR_SUBTASK_FAILED: return "SUBTASK_FAILED";
        case RCOG_ERROR_TOOL_ACCESS_DENIED: return "TOOL_ACCESS_DENIED";
        case RCOG_ERROR_TOOL_NOT_FOUND: return "TOOL_NOT_FOUND";
        case RCOG_ERROR_ANSWER_NOT_READY: return "ANSWER_NOT_READY";
        case RCOG_ERROR_BIO_ASYNC_DISCONNECTED: return "BIO_ASYNC_DISCONNECTED";
        case RCOG_ERROR_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case RCOG_ERROR_WORKER_POOL_EXHAUSTED: return "WORKER_POOL_EXHAUSTED";
        case RCOG_ERROR_INVALID_QUERY: return "INVALID_QUERY";
        case RCOG_ERROR_COMPRESSION_FAILED: return "COMPRESSION_FAILED";
        case RCOG_ERROR_DECOMPRESSION_FAILED: return "DECOMPRESSION_FAILED";
        case RCOG_ERROR_PATTERN_QUARANTINED: return "PATTERN_QUARANTINED";
        case RCOG_ERROR_IMMUNE_SHUTDOWN: return "IMMUNE_SHUTDOWN";
        case RCOG_ERROR_SWARM_DISCONNECTED: return "SWARM_DISCONNECTED";
        case RCOG_ERROR_IMAGINATION_FAILED: return "IMAGINATION_FAILED";
        case RCOG_ERROR_KG_NOT_CONNECTED: return "KG_NOT_CONNECTED";
        case RCOG_ERROR_ALREADY_INITIALIZED: return "ALREADY_INITIALIZED";
        case RCOG_ERROR_NOT_INITIALIZED: return "NOT_INITIALIZED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Get data type name as string
 */
static inline const char* rcog_dtype_name(rcog_data_type_t dtype) {
    switch (dtype) {
        case RCOG_DTYPE_UNKNOWN: return "unknown";
        case RCOG_DTYPE_TEXT: return "text";
        case RCOG_DTYPE_BINARY: return "binary";
        case RCOG_DTYPE_TENSOR: return "tensor";
        case RCOG_DTYPE_JSON: return "json";
        case RCOG_DTYPE_GRAPH: return "graph";
        case RCOG_DTYPE_EMBEDDING: return "embedding";
        default: return "invalid";
    }
}

/**
 * @brief Get capability tier name as string
 */
static inline const char* rcog_tier_name(rcog_capability_tier_t tier) {
    switch (tier) {
        case RCOG_TIER_ROOT: return "root";
        case RCOG_TIER_L1_REASONING: return "L1_reasoning";
        case RCOG_TIER_L2_PERCEPTION: return "L2_perception";
        case RCOG_TIER_L3_ACTION: return "L3_action";
        case RCOG_TIER_L4_SPECIALIZED: return "L4_specialized";
        default: return "invalid";
    }
}

/**
 * @brief Get goal type name as string
 */
static inline const char* rcog_goal_type_name(rcog_goal_type_t type) {
    switch (type) {
        case RCOG_GOAL_QUESTION_ANSWERING: return "question_answering";
        case RCOG_GOAL_SUMMARIZATION: return "summarization";
        case RCOG_GOAL_EXTRACTION: return "extraction";
        case RCOG_GOAL_REASONING: return "reasoning";
        case RCOG_GOAL_PLANNING: return "planning";
        case RCOG_GOAL_ANALYSIS: return "analysis";
        case RCOG_GOAL_GENERATION: return "generation";
        case RCOG_GOAL_TRANSLATION: return "translation";
        case RCOG_GOAL_VALIDATION: return "validation";
        case RCOG_GOAL_CUSTOM: return "custom";
        default: return "invalid";
    }
}

/**
 * @brief Get subtask status name as string
 */
static inline const char* rcog_subtask_status_name(rcog_subtask_status_t status) {
    switch (status) {
        case RCOG_SUBTASK_PENDING: return "pending";
        case RCOG_SUBTASK_QUEUED: return "queued";
        case RCOG_SUBTASK_RUNNING: return "running";
        case RCOG_SUBTASK_COMPLETED: return "completed";
        case RCOG_SUBTASK_FAILED: return "failed";
        case RCOG_SUBTASK_CANCELLED: return "cancelled";
        case RCOG_SUBTASK_TIMEOUT: return "timeout";
        default: return "invalid";
    }
}

/**
 * @brief Get answer status name as string
 */
static inline const char* rcog_answer_status_name(rcog_answer_status_t status) {
    switch (status) {
        case RCOG_ANSWER_INITIALIZING: return "initializing";
        case RCOG_ANSWER_REFINING: return "refining";
        case RCOG_ANSWER_CONVERGING: return "converging";
        case RCOG_ANSWER_READY: return "ready";
        case RCOG_ANSWER_STALLED: return "stalled";
        case RCOG_ANSWER_FAILED: return "failed";
        default: return "invalid";
    }
}

/**
 * @brief Free query result data if owned
 */
static inline void rcog_query_result_free(rcog_query_result_t* result) {
    if (result && result->owns_data && result->data) {
        nimcp_free(result->data);
        result->data = NULL;
        result->owns_data = false;
    }
}

/**
 * @brief Initialize default query parameters
 */
static inline rcog_query_params_t rcog_query_params_default(void) {
    rcog_query_params_t params = {0};
    params.output_limit = RCOG_DEFAULT_OUTPUT_LIMIT;
    return params;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RCOG_TYPES_H */
