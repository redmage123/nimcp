# Recursive Cognition Module - Design Document

**Version**: 1.2.0
**Date**: 2026-01-03
**Status**: Implementation Phase (Bridges Complete)
**Inspired By**: Prime Intellect Recursive Language Models (RLMs)

> **Deep Integrations**: Knowledge Graph, Collective Consciousness, Imagination Engine, Brain Immune System, Bio-Async

## Executive Summary

This document specifies the design for a **Recursive Cognition Module** that enables NIMCP to manage its own cognitive context through hierarchical delegation, inspired by Prime Intellect's Recursive Language Model paradigm. Instead of forcing the brain to process massive context in a single pass, the system treats input as an external environment and recursively delegates subtasks to specialized sub-processes.

## Background: Recursive Language Models

### Core RLM Concepts (Prime Intellect, 2025-2026)

| Concept | Description | NIMCP Analog |
|---------|-------------|--------------|
| **Context Folding** | Delegate context-heavy work to sub-processes instead of summarizing | Cortical hierarchy + bio-async delegation |
| **Environment-Based Input** | Input loaded into REPL, never directly in context | Working memory as queryable store |
| **Sub-LLM Batch** | `llm_batch()` dispatches parallel sub-tasks | Bio-async futures with phase coupling |
| **Answer Diffusion** | Iterative refinement via `{content, ready}` state | JEPA latent + executive threshold |
| **Tool Separation** | Root has REPL only; children get heavy tools | Hierarchical capability tiers |
| **Long-Horizon Agents** | Tasks spanning weeks to months | Persistent goal stacks + episodic memory |

### Why This Matters for NIMCP

Traditional neural architectures force all context through a fixed-size window. RLMs solve this by:

1. **Active Context Management**: The model decides what to read, when
2. **Hierarchical Decomposition**: Complex tasks split into coordinated subtasks
3. **Information Preservation**: No lossy summarization - original data always accessible
4. **Parallel Execution**: Independent subtasks run concurrently

NIMCP's bio-inspired architecture is uniquely suited for this pattern.

---

## System Integration Map

```
+=====================================================================================================================+
|                            NIMCP RECURSIVE COGNITION - SYSTEM INTEGRATION MAP v1.1                                  |
+=====================================================================================================================+
|                                                                                                                     |
|   +-------------------------------------------------------------------------------------------------------------+   |
|   |                                        DEEP INTEGRATION LAYER                                                |   |
|   |   +-------------------+  +-------------------+  +-------------------+  +-------------------+                 |   |
|   |   |  BRAIN KG         |  |  COLLECTIVE       |  |  IMAGINATION      |  |  BRAIN IMMUNE    |                 |   |
|   |   |  (Self-Awareness) |  |  CONSCIOUSNESS    |  |  ENGINE           |  |  SYSTEM          |                 |   |
|   |   |                   |  |  (Swarm)          |  |  (Hypothetical)   |  |  (Health)        |                 |   |
|   |   | > Module registry |  | > CRDT workspace  |  | > Mental rehearsal|  | > Cytokine mod   |                 |   |
|   |   | > Introspection   |  | > Stigmergy       |  | > Counterfactual  |  | > Inflammation   |                 |   |
|   |   | > KG Reader       |  | > IIT coherence   |  | > Prospective sim |  | > Degraded mode  |                 |   |
|   |   +--------+----------+  +--------+----------+  +--------+----------+  +--------+----------+                 |   |
|   |            |                      |                      |                      |                             |   |
|   +------------|----------------------|----------------------|----------------------|-----------------------------+   |
|                |                      |                      |                      |                                 |
|                +----------------------+----------------------+----------------------+                                 |
|                                                    |                                                                  |
|                                                    v                                                                  |
+=====================================================================================================================+
|                                                                                                              |
|  +--------------------------------------------------------------------------------------------------------+  |
|  |                                     RECURSIVE COGNITION ENGINE                                         |  |
|  |                                   nimcp_recursive_cognition.h                                          |  |
|  |                                                                                                        |  |
|  |  +---------------------------+  +---------------------------+  +---------------------------+           |  |
|  |  |     CONTEXT STORE         |  |      ORCHESTRATOR         |  |     DELEGATION POOL       |           |  |
|  |  |                           |  |                           |  |                           |           |  |
|  |  | . Environment variables   |  | . Task decomposition      |  | . Sub-cognition workers   |           |  |
|  |  | . Query-based access      |  | . Priority scheduling     |  | . Parallel execution      |           |  |
|  |  | . Slice/range retrieval   |  | . Depth limit tracking    |  | . Result aggregation      |           |  |
|  |  | . No direct injection     |  | . Answer state machine    |  | . Phase synchronization   |           |  |
|  |  +-------------+-------------+  +-------------+-------------+  +-------------+-------------+           |  |
|  |                |                              |                              |                          |  |
|  |                +------------------------------+------------------------------+                          |  |
|  |                                               |                                                         |  |
|  |  +---------------------------+  +-------------v-------------+  +---------------------------+           |  |
|  |  |     ANSWER REFINER        |  |      TOOL ROUTER          |  |     CAPABILITY TIERS      |           |  |
|  |  |                           |  |                           |  |                           |           |  |
|  |  | . Diffusion-style update  |  | . Route tools to level    |  | . ROOT: coordination only |           |  |
|  |  | . Confidence tracking     |  | . Prevent token bloat     |  | . L1: memory + reasoning  |           |  |
|  |  | . Ready threshold check   |  | . Sub-worker tool access  |  | . L2: perception + action |           |  |
|  |  +---------------------------+  +---------------------------+  +---------------------------+           |  |
|  +--------------------------------------------------------------------------------------------------------+  |
|                                               |                                                              |
|  ============================================|=============================================================  |
|                                               |                                                              |
|       +--------------------------------------+|+--------------------------------------+                       |
|       |                                      ||                                      |                       |
|       v                                      v|                                      v                       |
|  +==================+    +==================+||+==================+    +==================+                  |
|  | EXECUTIVE LAYER  |    | MEMORY LAYER     |||   WORLD MODEL    |    | PERCEPTION LAYER |                  |
|  +==================+    +==================+||+==================+    +==================+                  |
|  |                  |    |                  |||                  |    |                  |                  |
|  | +--------------+ |    | +--------------+ ||| +--------------+ |    | +--------------+ |                  |
|  | | PREFRONTAL   | |    | | WORKING MEM  | ||| |    JEPA      | |    | | VISUAL CTX   | |                  |
|  | |              | |    | |              | ||| |              | |    | |              | |                  |
|  | | > Goal stack | |    | | > Env store  | ||| | > Prediction | |    | | > Scene parse| |                  |
|  | | > Inhibition | |    | | > Salience   | ||| | > Latent     | |    | | > Object det | |                  |
|  | | > Planning   | |    | | > Retrieval  | ||| | > Evolution  | |    | | > Feature ex | |                  |
|  | +--------------+ |    | +--------------+ ||| +--------------+ |    | +--------------+ |                  |
|  |                  |    |                  |||                  |    |                  |                  |
|  | +--------------+ |    | +--------------+ ||| +--------------+ |    | +--------------+ |                  |
|  | | EXECUTIVE FN | |    | | HIPPOCAMPUS  | ||| |  FEP/ACTIVE  | |    | | AUDIO CTX    | |                  |
|  | |              | |    | |              | ||| |  INFERENCE   | |    | |              | |                  |
|  | | > Task queue | |    | | > Episodic   | ||| |              | |    | | > Speech rec | |                  |
|  | | > Delegation | |    | | > Retrieval  | ||| | > Free energy| |    | | > Sound loc  | |                  |
|  | | > Depth ctrl | |    | | > Consolid.  | ||| | > Belief upd | |    | | > Music proc | |                  |
|  | +--------------+ |    | +--------------+ ||| +--------------+ |    | +--------------+ |                  |
|  +==================+    +==================+||+==================+    +==================+                  |
|                                              ||                                                              |
|  ============================================||=============================================================  |
|                                              ||                                                              |
|  +==================+    +==================+||+==================+    +==================+                  |
|  | REASONING LAYER  |    | COORDINATION    |||   LEARNING LAYER |    | INFRASTRUCTURE   |                  |
|  +==================+    +==================+||+==================+    +==================+                  |
|  |                  |    |                  |||                  |    |                  |                  |
|  | +--------------+ |    | +--------------+ ||| +--------------+ |    | +--------------+ |                  |
|  | | LOGIC GATES  | |    | | BIO-ASYNC    | ||| | TRAINING     | |    | | THALAMIC RTR | |                  |
|  | |              | |    | |              | ||| |              | |    | |              | |                  |
|  | | > AST parse  | |    | | > Futures    | ||| | > RL update  | |    | | > Attention  | |                  |
|  | | > Recursive  | |    | | > Phase sync | ||| | > Experience | |    | | > Routing    | |                  |
|  | | > Post-order | |    | | > Channels   | ||| | > Replay     | |    | | > Gating     | |                  |
|  | +--------------+ |    | +--------------+ ||| +--------------+ |    | +--------------+ |                  |
|  |                  |    |                  |||                  |    |                  |                  |
|  | +--------------+ |    | +--------------+ ||| +--------------+ |    | +--------------+ |                  |
|  | | THEORY MIND  | |    | | KURAMOTO     | ||| | PLASTICITY   | |    | | GPU CONTEXT  | |                  |
|  | |              | |    | |              | ||| |              | |    | |              | |                  |
|  | | > BDI model  | |    | | > Oscillator | ||| | > STDP       | |    | | > Batch proc | |                  |
|  | | > Beliefs    | |    | | > Coupling   | ||| | > BCM        | |    | | > Parallel   | |                  |
|  | | > Predict    | |    | | > Coherence  | ||| | > Homeo      | |    | | > Kernels    | |                  |
|  | +--------------+ |    | +--------------+ ||| +--------------+ |    | +--------------+ |                  |
|  +==================+    +==================+||+==================+    +==================+                  |
|                                                                                                              |
+==============================================================================================================+
```

---

## Core Components

### 1. Context Store (`nimcp_rcog_context_store.h`)

The context store implements RLM's "environment as external variable" pattern. Input data is stored but **never directly injected** into the processing context.

```c
/**
 * Context store configuration
 */
typedef struct {
    size_t max_variables;           // Max named variables (default: 64)
    size_t max_variable_size;       // Max size per variable in bytes (default: 1MB)
    size_t output_limit_per_turn;   // Max chars shown per query (default: 8192)
    bool enable_persistence;        // Persist across sessions
    bool enable_compression;        // LZ4 compress large variables
} rcog_context_store_config_t;

/**
 * Variable access patterns (mirrors RLM REPL patterns)
 */
typedef enum {
    RCOG_ACCESS_SLICE,              // Get range: var[start:end]
    RCOG_ACCESS_SEARCH,             // Search for pattern in var
    RCOG_ACCESS_HEAD,               // First N items
    RCOG_ACCESS_TAIL,               // Last N items
    RCOG_ACCESS_SAMPLE,             // Random sample of N items
    RCOG_ACCESS_AGGREGATE           // Apply aggregation function
} rcog_access_pattern_t;

/**
 * Context store opaque handle
 */
typedef struct rcog_context_store rcog_context_store_t;

/**
 * Store a named variable (input data)
 */
nimcp_error_t rcog_context_store_set(
    rcog_context_store_t* store,
    const char* name,
    const void* data,
    size_t size,
    rcog_data_type_t dtype
);

/**
 * Query variable with access pattern (output limited)
 */
nimcp_error_t rcog_context_store_query(
    rcog_context_store_t* store,
    const char* name,
    rcog_access_pattern_t pattern,
    const rcog_query_params_t* params,
    rcog_query_result_t* result
);

/**
 * Execute helper function on variable (like RLM Python helpers)
 */
nimcp_error_t rcog_context_store_exec(
    rcog_context_store_t* store,
    const char* variable_name,
    rcog_helper_fn helper,
    void* helper_context,
    rcog_query_result_t* result
);
```

### 2. Orchestrator (`nimcp_rcog_orchestrator.h`)

The orchestrator coordinates task decomposition and manages the recursive call tree. This maps to RLM's "root model" that never directly sees input but coordinates sub-workers.

```c
/**
 * Orchestrator configuration
 */
typedef struct {
    size_t max_recursion_depth;     // Prevent unbounded recursion (default: 16)
    size_t max_parallel_subtasks;   // Max concurrent sub-workers (default: 8)
    float ready_threshold;          // Answer confidence threshold (default: 0.95)
    uint32_t max_refinement_steps;  // Max answer diffusion iterations (default: 32)
    bool enable_early_termination;  // Stop when confident
    rcog_scheduling_policy_t policy;// Priority, round-robin, or adaptive
} rcog_orchestrator_config_t;

/**
 * Task decomposition result
 */
typedef struct {
    size_t num_subtasks;
    rcog_subtask_t* subtasks;       // Array of subtasks
    rcog_dependency_graph_t* deps;  // DAG of dependencies
    float estimated_complexity;     // For resource allocation
} rcog_decomposition_t;

/**
 * Answer state (RLM's {content, ready} pattern)
 */
typedef struct {
    nimcp_tensor_t* content;        // Current answer representation (JEPA latent)
    float confidence;               // Current confidence level
    bool ready;                     // Ready for output
    uint32_t refinement_step;       // Current iteration
    rcog_answer_history_t* history; // Refinement trajectory
} rcog_answer_state_t;

/**
 * Orchestrator opaque handle
 */
typedef struct rcog_orchestrator rcog_orchestrator_t;

/**
 * Decompose a high-level goal into subtasks
 */
nimcp_error_t rcog_orchestrator_decompose(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    const rcog_context_store_t* context,
    rcog_decomposition_t* result
);

/**
 * Dispatch subtasks to delegation pool (like llm_batch)
 */
nimcp_error_t rcog_orchestrator_dispatch(
    rcog_orchestrator_t* orch,
    const rcog_decomposition_t* decomp,
    rcog_batch_handle_t* handle
);

/**
 * Aggregate results from subtasks
 */
nimcp_error_t rcog_orchestrator_aggregate(
    rcog_orchestrator_t* orch,
    rcog_batch_handle_t* handle,
    rcog_subtask_result_t* results,
    size_t num_results,
    rcog_answer_state_t* answer
);

/**
 * Refine answer (one diffusion step)
 */
nimcp_error_t rcog_orchestrator_refine(
    rcog_orchestrator_t* orch,
    rcog_answer_state_t* answer,
    const rcog_context_store_t* context
);
```

### 3. Delegation Pool (`nimcp_rcog_delegation.h`)

The delegation pool manages sub-cognition workers that execute subtasks in parallel. This implements RLM's `llm_batch()` pattern using bio-async infrastructure.

```c
/**
 * Delegation pool configuration
 */
typedef struct {
    size_t pool_size;               // Number of worker slots (default: 8)
    size_t worker_context_size;     // Context window per worker (default: 4096)
    rcog_capability_tier_t default_tier; // Default capability level
    bool enable_work_stealing;      // Load balancing
    nimcp_bio_async_t* bio_async;   // Bio-async for coordination
} rcog_delegation_config_t;

/**
 * Capability tiers (RLM's tool separation)
 */
typedef enum {
    RCOG_TIER_ROOT,                 // Coordination only (no tools)
    RCOG_TIER_L1_REASONING,         // Memory access, logic, planning
    RCOG_TIER_L2_PERCEPTION,        // Sensory processing, feature extraction
    RCOG_TIER_L3_ACTION,            // Motor control, output generation
    RCOG_TIER_L4_SPECIALIZED        // Domain-specific tools (vision, audio, etc.)
} rcog_capability_tier_t;

/**
 * Subtask specification
 */
typedef struct {
    uint64_t task_id;
    rcog_goal_t goal;               // What to accomplish
    rcog_capability_tier_t tier;    // Required capability level
    const char** context_refs;      // Variables to access from store
    size_t num_context_refs;
    float priority;                 // Scheduling priority
    uint32_t timeout_ms;            // Timeout for this subtask
    rcog_subtask_t* parent;         // For tracking recursion depth
} rcog_subtask_t;

/**
 * Batch handle for tracking parallel execution
 */
typedef struct rcog_batch_handle rcog_batch_handle_t;

/**
 * Delegation pool opaque handle
 */
typedef struct rcog_delegation_pool rcog_delegation_pool_t;

/**
 * Submit batch of subtasks (parallel execution)
 */
nimcp_error_t rcog_delegation_submit_batch(
    rcog_delegation_pool_t* pool,
    rcog_subtask_t* subtasks,
    size_t num_subtasks,
    rcog_batch_handle_t** handle
);

/**
 * Wait for batch completion with timeout
 */
nimcp_error_t rcog_delegation_await_batch(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle,
    uint32_t timeout_ms,
    rcog_batch_status_t* status
);

/**
 * Collect results from completed batch
 */
nimcp_error_t rcog_delegation_collect_results(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* handle,
    rcog_subtask_result_t* results,
    size_t* num_results
);

/**
 * Spawn recursive sub-orchestrator (for nested decomposition)
 */
nimcp_error_t rcog_delegation_spawn_sub_orchestrator(
    rcog_delegation_pool_t* pool,
    const rcog_subtask_t* parent_task,
    rcog_orchestrator_t** sub_orch
);
```

### 4. Answer Refiner (`nimcp_rcog_answer.h`)

Implements RLM's "answer generation via diffusion" pattern where the answer is iteratively refined across reasoning steps.

```c
/**
 * Answer refiner configuration
 */
typedef struct {
    size_t latent_dim;              // JEPA latent dimension
    float learning_rate;            // Refinement step size
    float momentum;                 // Momentum for smoother convergence
    float ready_threshold;          // Confidence threshold for ready=true
    uint32_t min_steps;             // Minimum refinement iterations
    uint32_t max_steps;             // Maximum refinement iterations
    bool enable_early_stopping;     // Stop when converged
    float convergence_epsilon;      // Convergence threshold
} rcog_answer_config_t;

/**
 * Answer refiner opaque handle
 */
typedef struct rcog_answer_refiner rcog_answer_refiner_t;

/**
 * Initialize answer state from goal
 */
nimcp_error_t rcog_answer_init(
    rcog_answer_refiner_t* refiner,
    const rcog_goal_t* goal,
    rcog_answer_state_t* state
);

/**
 * Single refinement step (diffusion iteration)
 */
nimcp_error_t rcog_answer_step(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state,
    const rcog_subtask_result_t* new_evidence,
    size_t num_evidence
);

/**
 * Check if answer is ready
 */
bool rcog_answer_is_ready(
    const rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state
);

/**
 * Extract final answer tensor
 */
nimcp_error_t rcog_answer_extract(
    rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state,
    nimcp_tensor_t** output
);

/**
 * Get confidence score
 */
float rcog_answer_get_confidence(
    const rcog_answer_state_t* state
);
```

### 5. Tool Router (`nimcp_rcog_tools.h`)

Implements RLM's tool separation where different capability tiers have access to different tools.

```c
/**
 * Tool specification
 */
typedef struct {
    const char* name;
    const char* description;
    rcog_capability_tier_t min_tier; // Minimum tier to access this tool
    rcog_tool_fn execute;            // Tool execution function
    void* tool_context;              // Tool-specific context
    bool returns_tensor;             // Output type
    size_t max_output_tokens;        // Limit verbose outputs
} rcog_tool_spec_t;

/**
 * Tool router opaque handle
 */
typedef struct rcog_tool_router rcog_tool_router_t;

/**
 * Register a tool with tier restriction
 */
nimcp_error_t rcog_tools_register(
    rcog_tool_router_t* router,
    const rcog_tool_spec_t* spec
);

/**
 * Get available tools for a capability tier
 */
nimcp_error_t rcog_tools_get_available(
    rcog_tool_router_t* router,
    rcog_capability_tier_t tier,
    rcog_tool_spec_t** tools,
    size_t* num_tools
);

/**
 * Execute tool (checks tier permission)
 */
nimcp_error_t rcog_tools_execute(
    rcog_tool_router_t* router,
    rcog_capability_tier_t caller_tier,
    const char* tool_name,
    const nimcp_tensor_t* input,
    rcog_tool_result_t* result
);
```

---

## Main Engine API (`nimcp_recursive_cognition.h`)

```c
/**
 * Recursive cognition engine configuration
 */
typedef struct {
    rcog_context_store_config_t context_config;
    rcog_orchestrator_config_t orchestrator_config;
    rcog_delegation_config_t delegation_config;
    rcog_answer_config_t answer_config;

    // Brain integrations
    nimcp_bio_async_t* bio_async;           // Required: for coordination
    nimcp_working_memory_t* working_memory; // Optional: for context store backing
    nimcp_executive_t* executive;           // Optional: for task management
    jepa_predictor_t* jepa;                 // Optional: for latent representations

    // Resource limits
    size_t max_total_context_bytes;         // Memory limit
    uint32_t global_timeout_ms;             // Overall timeout
} rcog_engine_config_t;

/**
 * Recursive cognition engine opaque handle
 */
typedef struct rcog_engine rcog_engine_t;

// ============================================================================
// Lifecycle
// ============================================================================

/**
 * Create recursive cognition engine
 */
rcog_engine_t* rcog_engine_create(const rcog_engine_config_t* config);

/**
 * Destroy engine and free resources
 */
void rcog_engine_destroy(rcog_engine_t* engine);

/**
 * Initialize engine for a brain instance
 */
nimcp_error_t rcog_engine_init_for_brain(
    brain_t brain,
    const rcog_engine_config_t* config
);

/**
 * Get engine from brain
 */
rcog_engine_t* brain_get_rcog_engine(brain_t brain);

// ============================================================================
// Context Management (RLM Environment Pattern)
// ============================================================================

/**
 * Load input data as environment variable
 */
nimcp_error_t rcog_load_context(
    rcog_engine_t* engine,
    const char* variable_name,
    const void* data,
    size_t size,
    rcog_data_type_t dtype
);

/**
 * Load tensor as environment variable
 */
nimcp_error_t rcog_load_context_tensor(
    rcog_engine_t* engine,
    const char* variable_name,
    const nimcp_tensor_t* tensor
);

/**
 * Clear all context variables
 */
nimcp_error_t rcog_clear_context(rcog_engine_t* engine);

// ============================================================================
// Recursive Processing (Main API)
// ============================================================================

/**
 * Process goal recursively (main entry point)
 *
 * This is the high-level API that:
 * 1. Decomposes the goal into subtasks
 * 2. Dispatches subtasks to the delegation pool
 * 3. Aggregates results
 * 4. Refines answer until ready
 *
 * @param engine The recursive cognition engine
 * @param goal The high-level goal to accomplish
 * @param output Output tensor (allocated by caller)
 * @return NIMCP_OK on success
 */
nimcp_error_t rcog_process(
    rcog_engine_t* engine,
    const rcog_goal_t* goal,
    nimcp_tensor_t** output
);

/**
 * Process with streaming callbacks (for long-horizon tasks)
 */
nimcp_error_t rcog_process_streaming(
    rcog_engine_t* engine,
    const rcog_goal_t* goal,
    rcog_stream_callback_t on_progress,
    rcog_stream_callback_t on_subtask_complete,
    void* user_data,
    nimcp_tensor_t** output
);

/**
 * Process asynchronously (returns immediately)
 */
nimcp_error_t rcog_process_async(
    rcog_engine_t* engine,
    const rcog_goal_t* goal,
    rcog_async_handle_t** handle
);

/**
 * Wait for async processing to complete
 */
nimcp_error_t rcog_await(
    rcog_engine_t* engine,
    rcog_async_handle_t* handle,
    uint32_t timeout_ms,
    nimcp_tensor_t** output
);

// ============================================================================
// Fine-Grained Control
// ============================================================================

/**
 * Manually decompose goal (for custom orchestration)
 */
nimcp_error_t rcog_decompose(
    rcog_engine_t* engine,
    const rcog_goal_t* goal,
    rcog_decomposition_t** decomp
);

/**
 * Manually dispatch subtasks
 */
nimcp_error_t rcog_dispatch(
    rcog_engine_t* engine,
    const rcog_decomposition_t* decomp,
    rcog_batch_handle_t** handle
);

/**
 * Manually aggregate and refine
 */
nimcp_error_t rcog_aggregate_and_refine(
    rcog_engine_t* engine,
    rcog_batch_handle_t* handle,
    rcog_answer_state_t* answer
);

// ============================================================================
// Introspection
// ============================================================================

/**
 * Get current recursion depth
 */
size_t rcog_get_current_depth(const rcog_engine_t* engine);

/**
 * Get active subtask count
 */
size_t rcog_get_active_subtasks(const rcog_engine_t* engine);

/**
 * Get processing statistics
 */
nimcp_error_t rcog_get_stats(
    const rcog_engine_t* engine,
    rcog_stats_t* stats
);

/**
 * Get execution trace (for debugging)
 */
nimcp_error_t rcog_get_trace(
    const rcog_engine_t* engine,
    rcog_trace_t** trace
);
```

---

## Bio-Async Integration

The recursive cognition module integrates deeply with NIMCP's bio-async system for coordination.

### Module ID Allocation

| Component | Module ID |
|-----------|-----------|
| Recursive Cognition Engine | 0x1400 |
| Context Store | 0x1401 |
| Orchestrator | 0x1402 |
| Delegation Pool | 0x1403 |
| Answer Refiner | 0x1404 |
| Tool Router | 0x1405 |

### Message Types

```c
typedef enum {
    // Engine messages (0x1400xx)
    RCOG_MSG_ENGINE_STARTED         = 0x140001,
    RCOG_MSG_ENGINE_STOPPED         = 0x140002,
    RCOG_MSG_GOAL_RECEIVED          = 0x140003,
    RCOG_MSG_PROCESSING_COMPLETE    = 0x140004,

    // Context store messages (0x1401xx)
    RCOG_MSG_CONTEXT_LOADED         = 0x140101,
    RCOG_MSG_CONTEXT_QUERIED        = 0x140102,
    RCOG_MSG_CONTEXT_CLEARED        = 0x140103,

    // Orchestrator messages (0x1402xx)
    RCOG_MSG_DECOMPOSITION_START    = 0x140201,
    RCOG_MSG_DECOMPOSITION_COMPLETE = 0x140202,
    RCOG_MSG_DEPTH_LIMIT_REACHED    = 0x140203,

    // Delegation messages (0x1403xx)
    RCOG_MSG_BATCH_SUBMITTED        = 0x140301,
    RCOG_MSG_SUBTASK_STARTED        = 0x140302,
    RCOG_MSG_SUBTASK_COMPLETED      = 0x140303,
    RCOG_MSG_BATCH_COMPLETED        = 0x140304,

    // Answer messages (0x1404xx)
    RCOG_MSG_REFINEMENT_STEP        = 0x140401,
    RCOG_MSG_ANSWER_READY           = 0x140402,
    RCOG_MSG_CONVERGENCE_ACHIEVED   = 0x140403,

    // Tool messages (0x1405xx)
    RCOG_MSG_TOOL_INVOKED           = 0x140501,
    RCOG_MSG_TOOL_COMPLETED         = 0x140502,
    RCOG_MSG_TOOL_ACCESS_DENIED     = 0x140503
} rcog_message_type_t;
```

### Neuromodulator Mapping

| Channel | Usage in Recursive Cognition |
|---------|------------------------------|
| Dopamine | Subtask completion signals, answer confidence |
| Norepinephrine | Task priority, urgency signaling |
| Acetylcholine | Attention to specific context variables |
| Serotonin | Overall processing state, mood |

---

## Integration Points

### Core NIMCP Components

| Component | Integration | Purpose |
|-----------|-------------|---------|
| **Working Memory** | Context store backing | Persistent variable storage |
| **Executive Functions** | Orchestrator | Task queue, priority management |
| **JEPA Predictor** | Answer refiner | Latent space representations |
| **Cortical Hierarchy** | Delegation pool | Hierarchical capability tiers |
| **Bio-Async** | All components | Coordination and messaging |
| **Hippocampus** | Context store | Episodic memory integration |
| **Thalamic Router** | Tool router | Attention-gated tool access |
| **Global Workspace** | Orchestrator | Broadcast important results |
| **Brain Knowledge Graph** | Self-awareness | Module registration, introspection |
| **Collective Workspace** | Swarm delegation | Distributed recursive processing |
| **Imagination Engine** | Hypothetical reasoning | Mental simulation of subtasks |
| **Brain Immune System** | Health monitoring | Modulate capacity under stress |

---

## Deep Integration: Internal Knowledge Graph

The recursive cognition module integrates with both the **Brain KG** (structural self-awareness) and **KG Reader** (semantic knowledge).

### Brain KG Registration

```c
/**
 * Register recursive cognition in brain's internal knowledge graph
 */
typedef struct {
    brain_kg_t* kg;                     // Brain KG handle
    brain_kg_node_id_t engine_node;     // Engine node ID
    brain_kg_node_id_t context_node;    // Context store node ID
    brain_kg_node_id_t orchestrator_node;
    brain_kg_node_id_t delegation_node;
} rcog_kg_integration_t;

/**
 * Register engine with brain knowledge graph for introspection
 */
nimcp_error_t rcog_engine_register_kg(
    rcog_engine_t* engine,
    brain_kg_t* kg
);

/**
 * Query engine capabilities via KG (for self-model)
 */
nimcp_error_t rcog_engine_kg_query_capabilities(
    rcog_engine_t* engine,
    rcog_capability_list_t* capabilities
);

/**
 * Update KG with current processing state (for introspection)
 */
nimcp_error_t rcog_engine_kg_update_state(
    rcog_engine_t* engine,
    const rcog_processing_state_t* state
);
```

### KG Node Types for Recursive Cognition

| Node Type | Description | Metadata |
|-----------|-------------|----------|
| `BRAIN_KG_NODE_COGNITIVE` | Recursive cognition engine | max_depth, active_subtasks |
| `BRAIN_KG_NODE_UTILITY` | Context store | variable_count, memory_usage |
| `BRAIN_KG_NODE_COORDINATOR` | Orchestrator | decomposition_strategy |
| `BRAIN_KG_NODE_INTEGRATION` | Delegation pool | pool_size, active_workers |

### KG Edge Relationships

```c
// Engine → Context Store: PROVIDES_TO (context variables)
// Orchestrator → Delegation Pool: COORDINATES_WITH
// Delegation Pool → Workers: SENDS_TO
// Engine → JEPA: INTEGRATES_WITH (latent representations)
// Engine → Imagination: INTEGRATES_WITH (hypothetical simulation)
```

### Semantic Knowledge Integration

The context store can query the **KG Reader** for semantic knowledge:

```c
/**
 * Connect to KG Reader for semantic knowledge access
 */
nimcp_error_t rcog_context_connect_kg_reader(
    rcog_context_store_t* store,
    kg_reader_t* kg_reader
);

/**
 * Query semantic knowledge as context variable
 *
 * Loads relevant KG entities matching query into context store
 * as a queryable variable for subtasks.
 */
nimcp_error_t rcog_context_load_semantic_knowledge(
    rcog_context_store_t* store,
    const char* variable_name,
    const char* query,
    size_t max_entities
);
```

---

## Deep Integration: Collective Consciousness / Swarm

The recursive cognition module can distribute processing across a swarm using the **Collective Workspace** for shared state and **Swarm Consciousness** for coordination.

### Collective Workspace Integration

```c
/**
 * Collective cognition configuration
 */
typedef struct {
    collective_workspace_t* workspace;   // Shared workspace
    swarm_consciousness_t* consciousness; // IIT-based coordination
    uint16_t drone_id;                   // This drone's ID
    uint16_t swarm_size;                 // Total swarm members
    float broadcast_threshold;           // Salience for sharing (default: 0.6)
    bool enable_stigmergy;               // Pheromone-like hints
} rcog_collective_config_t;

/**
 * Connect engine to collective workspace for distributed processing
 */
nimcp_error_t rcog_engine_connect_collective(
    rcog_engine_t* engine,
    const rcog_collective_config_t* config
);

/**
 * Distribute subtask across swarm (collective delegation)
 *
 * Instead of local delegation pool, broadcast subtask to swarm.
 * Other drones can volunteer to process based on their capacity.
 */
nimcp_error_t rcog_delegation_broadcast_subtask(
    rcog_delegation_pool_t* pool,
    const rcog_subtask_t* subtask,
    rcog_collective_handle_t* handle
);

/**
 * Collect results from swarm members
 * Uses CRDT merging for conflict-free aggregation
 */
nimcp_error_t rcog_delegation_collect_swarm_results(
    rcog_delegation_pool_t* pool,
    rcog_collective_handle_t* handle,
    rcog_subtask_result_t* results,
    size_t* num_results
);
```

### Swarm-Aware Context Store

```c
/**
 * Share context variable with swarm (stigmergy pattern)
 *
 * Other drones can query this variable via collective workspace.
 */
nimcp_error_t rcog_context_share_with_swarm(
    rcog_context_store_t* store,
    const char* variable_name,
    float salience
);

/**
 * Import shared context from swarm member
 */
nimcp_error_t rcog_context_import_from_swarm(
    rcog_context_store_t* store,
    uint16_t source_drone_id,
    const char* variable_name
);
```

### Collective Answer Refinement

```c
/**
 * Distributed answer diffusion across swarm
 *
 * Each drone refines locally, then shares via collective workspace.
 * Consensus emerges through CRDT merging of answer states.
 */
nimcp_error_t rcog_answer_collective_refine(
    rcog_answer_refiner_t* refiner,
    rcog_answer_state_t* state,
    collective_workspace_t* workspace
);

/**
 * Check if swarm has reached consensus on answer
 * Uses swarm consciousness coherence metric
 */
bool rcog_answer_swarm_consensus_reached(
    rcog_answer_refiner_t* refiner,
    const rcog_answer_state_t* state,
    swarm_consciousness_t* consciousness,
    float coherence_threshold
);
```

### Swarm Message Types

New bio-async message types for collective cognition:

```c
// Recursive cognition swarm messages (0x1406xx)
RCOG_MSG_SUBTASK_BROADCAST       = 0x140601,  // Broadcast subtask to swarm
RCOG_MSG_SUBTASK_VOLUNTEER       = 0x140602,  // Volunteer to process subtask
RCOG_MSG_SUBTASK_RESULT_SHARE    = 0x140603,  // Share result with swarm
RCOG_MSG_CONTEXT_SHARE           = 0x140604,  // Share context variable
RCOG_MSG_ANSWER_STATE_SYNC       = 0x140605,  // Sync answer state (CRDT)
RCOG_MSG_DEPTH_LIMIT_WARNING     = 0x140606,  // Warn swarm of depth limit
RCOG_MSG_COLLECTIVE_READY        = 0x140607,  // Answer ready signal
```

---

## Deep Integration: Imagination Engine

The imagination engine enables **hypothetical reasoning** - mentally simulating subtask outcomes before executing them.

### Imagination-Assisted Decomposition

```c
/**
 * Connect to imagination engine for hypothetical planning
 */
nimcp_error_t rcog_engine_connect_imagination(
    rcog_engine_t* engine,
    imagination_engine_t* imagination
);

/**
 * Simulate subtask outcomes before dispatching
 *
 * Uses imagination to predict which decomposition strategy
 * will yield best results, without actually executing.
 */
nimcp_error_t rcog_orchestrator_simulate_decomposition(
    rcog_orchestrator_t* orch,
    const rcog_goal_t* goal,
    rcog_decomposition_t** candidates,
    size_t num_candidates,
    rcog_simulation_result_t* results
);

/**
 * Counterfactual analysis: "What if we had decomposed differently?"
 */
nimcp_error_t rcog_orchestrator_counterfactual_analysis(
    rcog_orchestrator_t* orch,
    const rcog_decomposition_t* actual,
    const rcog_decomposition_t* alternative,
    rcog_counterfactual_result_t* result
);
```

### Imagination Modes for Recursive Cognition

| Mode | Usage in RCOG |
|------|---------------|
| `DIRECTED` | Goal-directed subtask simulation |
| `COUNTERFACTUAL` | "What if" decomposition analysis |
| `PROSPECTIVE` | Predict answer quality before refinement |
| `CREATIVE` | Generate novel decomposition strategies |

### Mental Rehearsal of Subtasks

```c
/**
 * Mental rehearsal: simulate subtask execution
 *
 * Uses imagination to predict subtask outcome without
 * actually consuming resources. Useful for:
 * - Pruning unlikely-to-succeed subtasks
 * - Estimating resource requirements
 * - Detecting potential deadlocks
 */
nimcp_error_t rcog_delegation_rehearse_subtask(
    rcog_delegation_pool_t* pool,
    const rcog_subtask_t* subtask,
    imagination_engine_t* imagination,
    rcog_rehearsal_result_t* result
);

/**
 * Imagine answer before full computation
 *
 * Quick approximate answer via imagination, useful for:
 * - Early termination if confidence is sufficient
 * - Guiding refinement direction
 */
nimcp_error_t rcog_answer_imagine_result(
    rcog_answer_refiner_t* refiner,
    const rcog_goal_t* goal,
    imagination_engine_t* imagination,
    nimcp_tensor_t** imagined_answer,
    float* predicted_confidence
);
```

### Imagination-Driven Tool Selection

```c
/**
 * Simulate tool usage before execution
 *
 * Imagine the result of using a tool to decide if it's
 * worth the cost of actual execution.
 */
nimcp_error_t rcog_tools_simulate_execution(
    rcog_tool_router_t* router,
    const char* tool_name,
    const nimcp_tensor_t* input,
    imagination_engine_t* imagination,
    rcog_tool_simulation_t* result
);
```

---

## Deep Integration: Brain Immune System

The immune system modulates recursive cognition capacity based on system health.

### Immune Modulation of Capacity

```c
/**
 * Connect to brain immune system for health-aware processing
 */
nimcp_error_t rcog_engine_connect_immune(
    rcog_engine_t* engine,
    brain_immune_system_t* immune
);

/**
 * Immune modulation effects on recursive cognition
 */
typedef struct {
    float capacity_multiplier;      // 0.0-1.0, reduces under inflammation
    float max_depth_multiplier;     // Reduce recursion depth when sick
    float parallelism_multiplier;   // Reduce parallel subtasks
    float timeout_multiplier;       // Increase timeouts (slower processing)
    bool enable_degraded_mode;      // Simplify decomposition strategy
} rcog_immune_modulation_t;

/**
 * Get current immune modulation effects
 */
nimcp_error_t rcog_engine_get_immune_modulation(
    rcog_engine_t* engine,
    rcog_immune_modulation_t* modulation
);
```

### Cytokine Effects on Processing

| Cytokine | Effect on Recursive Cognition |
|----------|-------------------------------|
| **IL-1β** (pro-inflammatory) | ↓ Working memory capacity, ↓ context store size |
| **IL-6** (acute phase) | ↓ Parallelism, ↑ timeouts |
| **TNF-α** (severe) | ↓ Max recursion depth, enable degraded mode |
| **IL-10** (anti-inflammatory) | Gradual recovery of capacity |
| **IFN-γ** (quarantine) | Isolate suspicious subtasks |

### Inflammation-Aware Scheduling

```c
/**
 * Adjust orchestrator based on inflammation level
 */
nimcp_error_t rcog_orchestrator_apply_immune_modulation(
    rcog_orchestrator_t* orch,
    brain_inflammation_level_t level
);

/**
 * Inflammation level effects:
 * - NONE: Full capacity
 * - LOCAL: Reduce parallelism by 20%
 * - REGIONAL: Reduce depth limit, 40% parallelism
 * - SYSTEMIC: Degraded mode, sequential only
 * - STORM: Emergency shutdown, return partial results
 */
```

### Immune Response to Subtask Failures

```c
/**
 * Report subtask failure as potential "antigen"
 *
 * Repeated failures from same decomposition pattern
 * trigger immune response (learn to avoid).
 */
nimcp_error_t rcog_immune_report_subtask_failure(
    rcog_engine_t* engine,
    const rcog_subtask_t* subtask,
    nimcp_error_t failure_reason
);

/**
 * Check if decomposition pattern is "quarantined"
 * (too many failures, immune system blocking)
 */
bool rcog_immune_is_pattern_quarantined(
    rcog_engine_t* engine,
    const rcog_decomposition_t* decomp
);
```

---

## Deep Integration: Bio-Async System

Bio-async provides the coordination layer for all recursive cognition operations.

### Neuromodulator Channel Semantics

| Channel | Usage in Recursive Cognition |
|---------|------------------------------|
| **Dopamine** | Subtask completion, answer refinement success |
| **Norepinephrine** | Priority escalation, timeout warnings |
| **Acetylcholine** | Context variable access, attention to subtask |
| **Serotonin** | Long-horizon patience, processing state |

### Phase Coupling for Subtask Synchronization

```c
/**
 * Create phase sync for parallel subtasks
 *
 * Uses Kuramoto oscillators to synchronize completion
 * of parallel subtasks before aggregation.
 */
nimcp_error_t rcog_delegation_create_phase_sync(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* batch,
    nimcp_oscillation_band_t band,
    nimcp_phase_sync_t** sync
);

/**
 * Wait for subtask batch to reach coherence
 *
 * More biologically realistic than simple barrier.
 * Allows partial results if some subtasks are slow.
 */
nimcp_error_t rcog_delegation_wait_phase_coherent(
    rcog_delegation_pool_t* pool,
    rcog_batch_handle_t* batch,
    float coherence_threshold,
    uint32_t timeout_ms
);
```

### Oscillation Band Selection

| Subtask Type | Recommended Band | Rationale |
|--------------|------------------|-----------|
| Fast perception | GAMMA (30-100 Hz) | Tight sync, quick binding |
| Working memory | BETA (12-30 Hz) | Moderate sync |
| Sequential reasoning | THETA (4-8 Hz) | Memory/sequence |
| Long-horizon planning | DELTA (0.5-4 Hz) | Slow, tolerant |

### Glial Wave Coordination

```c
/**
 * Initiate glial wave for system-wide state change
 *
 * Used for major mode transitions:
 * - Entering/exiting recursive processing
 * - Global resource reallocation
 * - Swarm-wide synchronization
 */
nimcp_error_t rcog_engine_initiate_glial_wave(
    rcog_engine_t* engine,
    rcog_state_transition_t transition,
    nimcp_glial_wave_t* wave
);

/**
 * State transitions that trigger glial waves:
 */
typedef enum {
    RCOG_TRANSITION_START_PROCESSING,     // Begin recursive processing
    RCOG_TRANSITION_DEPTH_LIMIT_REACHED,  // Cannot recurse further
    RCOG_TRANSITION_ANSWER_READY,         // Processing complete
    RCOG_TRANSITION_EMERGENCY_STOP,       // Abort all subtasks
    RCOG_TRANSITION_SWARM_HANDOFF         // Transfer to another drone
} rcog_state_transition_t;
```

### Predictive Coding for Context Access

```c
/**
 * Predictive model for context variable access
 *
 * Learn which variables are likely to be accessed together,
 * prefetch proactively to reduce latency.
 */
nimcp_error_t rcog_context_enable_predictive_access(
    rcog_context_store_t* store,
    nimcp_predictive_model_t* model
);

/**
 * Register callback for surprising access patterns
 *
 * Fires when actual access differs from prediction,
 * useful for debugging and optimization.
 */
nimcp_error_t rcog_context_on_access_surprise(
    rcog_context_store_t* store,
    nimcp_prediction_error_callback_t callback,
    void* user_data
);
```

### Bio-Async Future Semantics

```c
/**
 * Create bio-future for subtask completion
 *
 * Uses dopamine channel for reward-like completion signal.
 * Confidence decays over time if result not used.
 */
nimcp_error_t rcog_subtask_create_future(
    rcog_subtask_t* subtask,
    nimcp_bio_channel_type_t channel,
    nimcp_bio_future_t* future
);

/**
 * Check subtask confidence (decays biologically)
 */
float rcog_subtask_get_confidence(
    const rcog_subtask_result_t* result
);
```

---

## Connection Functions (Complete)

```c
// ============================================================================
// Brain Factory Integration
// ============================================================================

// Core components
int rcog_engine_connect_working_memory(rcog_engine_t* engine, nimcp_working_memory_t* wm);
int rcog_engine_connect_executive(rcog_engine_t* engine, nimcp_executive_t* exec);
int rcog_engine_connect_jepa(rcog_engine_t* engine, jepa_predictor_t* jepa);
int rcog_engine_connect_hippocampus(rcog_engine_t* engine, hippocampus_adapter_t* hipp);
int rcog_engine_connect_thalamic(rcog_engine_t* engine, thalamic_router_t* thalamic);
int rcog_engine_connect_cortical(rcog_engine_t* engine, cortical_hierarchy_t* cortex);
int rcog_engine_connect_global_workspace(rcog_engine_t* engine, global_workspace_t* gws);

// Bio-async
int rcog_engine_connect_bio_async(rcog_engine_t* engine);
int rcog_engine_disconnect_bio_async(rcog_engine_t* engine);
bool rcog_engine_is_bio_async_connected(const rcog_engine_t* engine);

// Knowledge Graph
int rcog_engine_connect_brain_kg(rcog_engine_t* engine, brain_kg_t* kg);
int rcog_engine_connect_kg_reader(rcog_engine_t* engine, kg_reader_t* kg_reader);

// Collective Consciousness / Swarm
int rcog_engine_connect_collective_workspace(rcog_engine_t* engine, collective_workspace_t* workspace);
int rcog_engine_connect_swarm_consciousness(rcog_engine_t* engine, swarm_consciousness_t* consciousness);

// Imagination Engine
int rcog_engine_connect_imagination(rcog_engine_t* engine, imagination_engine_t* imagination);

// Brain Immune System
int rcog_engine_connect_immune(rcog_engine_t* engine, brain_immune_system_t* immune);

// FEP Bridge (for precision weighting)
int rcog_engine_connect_fep(rcog_engine_t* engine, fep_active_inference_t* fep);
```

---

## Example Usage

### Basic Recursive Processing

```c
// Create engine with brain integration
rcog_engine_config_t config = rcog_engine_default_config();
config.bio_async = brain_get_bio_async(brain);
config.working_memory = brain_get_working_memory(brain);
config.executive = brain_get_executive(brain);
config.jepa = brain_get_jepa(brain);

rcog_engine_t* engine = rcog_engine_create(&config);

// Load context (large document, never directly in processing window)
const char* document = load_large_document("data.txt");
rcog_load_context(engine, "document", document, strlen(document), RCOG_DTYPE_TEXT);

// Define goal
rcog_goal_t goal = {
    .type = RCOG_GOAL_QUESTION_ANSWERING,
    .query = "What are the key findings in section 3?",
    .context_refs = (const char*[]){"document"},
    .num_context_refs = 1
};

// Process recursively (engine handles decomposition, delegation, refinement)
nimcp_tensor_t* answer = NULL;
rcog_error_t err = rcog_process(engine, &goal, &answer);

if (err == NIMCP_OK) {
    // Extract answer
    char* answer_text = rcog_tensor_to_text(answer);
    printf("Answer: %s\n", answer_text);
    free(answer_text);
    nimcp_tensor_destroy(answer);
}

rcog_engine_destroy(engine);
```

### Streaming Long-Horizon Task

```c
void on_progress(const rcog_progress_t* progress, void* user_data) {
    printf("Progress: %zu/%zu subtasks, confidence: %.2f\n",
           progress->completed_subtasks,
           progress->total_subtasks,
           progress->current_confidence);
}

void on_subtask_complete(const rcog_subtask_result_t* result, void* user_data) {
    printf("Subtask %llu completed: %s\n", result->task_id, result->summary);
}

rcog_goal_t goal = {
    .type = RCOG_GOAL_MULTI_STEP_REASONING,
    .query = "Analyze this codebase and suggest architectural improvements",
    .context_refs = (const char*[]){"source_files", "documentation", "tests"},
    .num_context_refs = 3,
    .max_duration_hours = 2  // Long-horizon task
};

nimcp_tensor_t* answer = NULL;
rcog_process_streaming(engine, &goal, on_progress, on_subtask_complete, NULL, &answer);
```

### Custom Orchestration

```c
// Manual decomposition for fine-grained control
rcog_decomposition_t* decomp = NULL;
rcog_decompose(engine, &goal, &decomp);

printf("Decomposed into %zu subtasks\n", decomp->num_subtasks);

// Modify subtasks if needed
for (size_t i = 0; i < decomp->num_subtasks; i++) {
    if (decomp->subtasks[i].tier == RCOG_TIER_L2_PERCEPTION) {
        decomp->subtasks[i].priority *= 2.0f;  // Boost perception tasks
    }
}

// Dispatch and wait
rcog_batch_handle_t* handle = NULL;
rcog_dispatch(engine, decomp, &handle);

rcog_answer_state_t answer_state;
rcog_aggregate_and_refine(engine, handle, &answer_state);

while (!rcog_answer_is_ready(engine->refiner, &answer_state)) {
    rcog_orchestrator_refine(engine->orchestrator, &answer_state, engine->context);
}

nimcp_tensor_t* output = NULL;
rcog_answer_extract(engine->refiner, &answer_state, &output);
```

---

## File Structure

### Headers (`include/cognitive/recursive/`)

| File | Purpose | Status |
|------|---------|--------|
| `nimcp_rcog_types.h` | Shared type definitions | ✅ Complete |
| `nimcp_rcog_context_store.h` | Environment variable storage | ✅ Complete |
| `nimcp_rcog_answer.h` | Answer diffusion refinement | ✅ Complete |
| `nimcp_rcog_bio_async_bridge.h` | Bio-async integration bridge | ✅ Complete |
| `nimcp_rcog_imagination_bridge.h` | Imagination engine bridge | ✅ Complete |
| `nimcp_rcog_immune_bridge.h` | Brain immune system bridge | ✅ Complete |
| `nimcp_rcog_collective_bridge.h` | Swarm/collective bridge | ✅ Complete |
| `nimcp_rcog_brain_kg_bridge.h` | Brain knowledge graph bridge | ✅ Complete |
| `nimcp_recursive_cognition.h` | Main engine API | Planned |
| `nimcp_rcog_orchestrator.h` | Task decomposition and coordination | Planned |
| `nimcp_rcog_delegation.h` | Parallel subtask execution | Planned |
| `nimcp_rcog_tools.h` | Tiered tool access | Planned |

### Implementation (`src/cognitive/recursive/`)

| File | Purpose | Status |
|------|---------|--------|
| `nimcp_rcog_context_store.c` | Context store implementation | ✅ Complete |
| `nimcp_rcog_answer.c` | Answer refiner implementation | ✅ Complete |
| `nimcp_rcog_bio_async_bridge.c` | Bio-async bridge implementation | ✅ Complete |
| `nimcp_rcog_imagination_bridge.c` | Imagination bridge implementation | ✅ Complete |
| `nimcp_rcog_immune_bridge.c` | Brain immune bridge implementation | ✅ Complete |
| `nimcp_rcog_collective_bridge.c` | Collective/swarm bridge implementation | ✅ Complete |
| `nimcp_rcog_brain_kg_bridge.c` | Brain KG bridge implementation | ✅ Complete |
| `nimcp_recursive_cognition.c` | Main engine implementation | Planned |
| `nimcp_rcog_orchestrator.c` | Orchestrator implementation | Planned |
| `nimcp_rcog_delegation.c` | Delegation pool implementation | Planned |
| `nimcp_rcog_tools.c` | Tool router implementation | Planned |

### GPU Kernels (`src/gpu/recursive/`)

| File | Purpose |
|------|---------|
| `nimcp_rcog_kernels.cu` | CUDA batch processing |
| `nimcp_rcog_aggregation.cu` | Parallel result aggregation |

### Tests (`test/unit/cognitive/recursive/`)

| File | Purpose |
|------|---------|
| `test_rcog_context_store.cpp` | Context store unit tests |
| `test_rcog_orchestrator.cpp` | Orchestrator unit tests |
| `test_rcog_delegation.cpp` | Delegation pool unit tests |
| `test_rcog_answer.cpp` | Answer refiner unit tests |
| `test_rcog_tools.cpp` | Tool router unit tests |
| `test_rcog_integration.cpp` | Component integration tests |
| `test_rcog_e2e.cpp` | End-to-end pipeline tests |

---

## Implementation Phases

### Phase 1: Core Infrastructure (Foundation)

**Components:**
- `rcog_types.h` - Type definitions and enums
- `rcog_context_store.h/.c` - Environment variable storage
- `rcog_answer.h/.c` - Answer state management

**Deliverables:**
- Context can be loaded and queried
- Answer diffusion loop works standalone
- Unit tests pass

### Phase 2: Orchestration

**Components:**
- `rcog_orchestrator.h/.c` - Task decomposition
- Basic decomposition strategies (sequential, parallel, hierarchical)

**Deliverables:**
- Goals decompose into subtasks
- Dependency graphs correctly computed
- Depth limits enforced

### Phase 3: Delegation

**Components:**
- `rcog_delegation.h/.c` - Worker pool
- Bio-async integration for futures

**Deliverables:**
- Subtasks execute in parallel
- Results collected correctly
- Phase coupling synchronizes workers

### Phase 4: Tool System

**Components:**
- `rcog_tools.h/.c` - Tool registry and routing
- Tier-based access control

**Deliverables:**
- Tools register with tier restrictions
- Access control enforced
- Built-in tools for memory, perception, reasoning

### Phase 5: Brain Integration

**Components:**
- `nimcp_recursive_cognition.h/.c` - Main engine
- Connection functions for all brain components

**Deliverables:**
- Engine initializes for brain
- All integrations functional
- Bio-async messages flow correctly

### Phase 6: GPU Acceleration

**Components:**
- CUDA kernels for batch processing
- Parallel aggregation

**Deliverables:**
- GPU-accelerated subtask processing
- Batch answer refinement

### Phase 7: Training Integration

**Components:**
- RL-based decomposition learning
- Experience replay from recursive traces

**Deliverables:**
- System learns better decomposition strategies
- Context folding optimizes over time

---

## Performance Considerations

### Memory Management

- Context store uses memory-mapped files for large variables
- LZ4 compression for infrequently accessed data
- Reference counting for shared tensors
- Pool allocation for subtask structures

### Parallel Execution

- Work-stealing scheduler for load balancing
- Lock-free queues for subtask submission
- Phase coupling prevents thundering herd
- Adaptive batch sizes based on complexity

### Latency Optimization

- Early termination when confidence sufficient
- Speculative execution for likely subtasks
- Caching of frequent decomposition patterns
- Warm worker pool (no cold start)

---

## Error Handling

### Error Codes

```c
typedef enum {
    RCOG_OK = 0,
    RCOG_ERROR_NULL_POINTER,
    RCOG_ERROR_INVALID_CONFIG,
    RCOG_ERROR_CONTEXT_NOT_FOUND,
    RCOG_ERROR_CONTEXT_TOO_LARGE,
    RCOG_ERROR_MAX_DEPTH_EXCEEDED,
    RCOG_ERROR_TIMEOUT,
    RCOG_ERROR_SUBTASK_FAILED,
    RCOG_ERROR_TOOL_ACCESS_DENIED,
    RCOG_ERROR_TOOL_NOT_FOUND,
    RCOG_ERROR_ANSWER_NOT_READY,
    RCOG_ERROR_BIO_ASYNC_DISCONNECTED,
    RCOG_ERROR_OUT_OF_MEMORY,
    RCOG_ERROR_WORKER_POOL_EXHAUSTED
} rcog_error_t;
```

### Recovery Strategies

| Error | Recovery |
|-------|----------|
| Subtask timeout | Retry with increased timeout, or skip and aggregate partial results |
| Max depth exceeded | Return partial answer with reduced confidence |
| Worker pool exhausted | Queue subtasks, process when workers available |
| Tool access denied | Log warning, continue without tool result |
| Context too large | Suggest chunking, or compress and retry |

---

## Testing Strategy

### Unit Tests

Each component tested in isolation:
- Context store: load, query, slice operations
- Orchestrator: decomposition correctness, depth limits
- Delegation: parallel execution, result collection
- Answer: refinement convergence, confidence calculation
- Tools: registration, tier enforcement

### Integration Tests

Component interactions:
- Orchestrator → Delegation flow
- Context → Orchestrator queries
- Delegation → Answer aggregation
- Full bio-async message flow

### E2E Tests

Complete pipelines:
- Simple question answering
- Multi-step reasoning
- Long document analysis
- Code understanding (eating own dogfood)

### Benchmarks

- Context load/query latency
- Decomposition throughput
- Parallel scaling efficiency
- Answer convergence rate

---

## References

1. [Prime Intellect RLM Blog](https://www.primeintellect.ai/blog/rlm) - Original RLM concept
2. [Prime-RL GitHub](https://github.com/PrimeIntellect-ai/prime-rl) - Async RL training framework
3. [INTELLECT-3 Technical Report](https://arxiv.org/html/2512.16144v1) - Large-scale RL training
4. NIMCP Bio-Async Documentation - `docs/claude/modules/bio-async.md`
5. NIMCP Executive Functions - `include/cognitive/nimcp_executive.h`
6. NIMCP Working Memory - `include/cognitive/nimcp_working_memory.h`
7. NIMCP JEPA - `include/cognitive/jepa/`

---

## Appendix A: Mapping RLM Concepts to NIMCP

| RLM Concept | NIMCP Implementation |
|-------------|---------------------|
| Python REPL environment | `rcog_context_store_t` with query API |
| `answer = {"content": "", "ready": False}` | `rcog_answer_state_t` with JEPA latent |
| `llm_batch()` | `rcog_delegation_submit_batch()` |
| Sub-LLM with tools | Workers at `RCOG_TIER_L2_PERCEPTION` and below |
| Root model (coordination only) | Orchestrator at `RCOG_TIER_ROOT` |
| Output character limit | `output_limit_per_turn` in context config |
| Context folding | Delegation to sub-workers instead of summarization |
| Answer diffusion | `rcog_answer_step()` iterative refinement |
| Long-horizon agents | Persistent goal stacks + streaming API |

---

## Appendix B: GOTCHAs

1. **Never inject context directly** - Always use `rcog_context_store_query()`, never pass raw data to workers
2. **Respect tier boundaries** - Workers cannot access tools above their tier
3. **Depth limits are hard** - `RCOG_ERROR_MAX_DEPTH_EXCEEDED` is not recoverable by retry
4. **Bio-async required** - Engine will not function without bio-async connection
5. **Answer may never be ready** - Set reasonable `max_refinement_steps` and handle partial results
6. **Context store is not working memory** - They can be connected but serve different purposes

---

## Changelog

- **1.2.0** (2026-01-03): Bridge implementations complete
  - Added 5 bidirectional bridge implementations for deep integration
  - Bio-async bridge: neuromodulators, phase coupling, glial waves
  - Imagination bridge: simulation, rehearsal, counterfactual analysis
  - Immune bridge: inflammation modulation, quarantine patterns
  - Collective bridge: stigmergy, subtask distribution, consensus
  - Brain KG bridge: self-model, introspection, semantic queries
- **1.1.0** (2026-01-03): Added deep integration specifications
- **1.0.0** (2026-01-03): Initial design document
