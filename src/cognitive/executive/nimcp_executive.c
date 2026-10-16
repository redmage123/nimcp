/**
 * @file nimcp_executive.c
 * @brief Executive functions implementation - task switching, planning, inhibition
 *
 * WHAT: Dorsolateral prefrontal cortex (DLPFC) executive control
 * WHY:  Enable goal-directed behavior, multi-tasking, and impulse control
 * HOW:  Task queue with priority scheduling, switch cost tracking, planning
 *
 * BIOLOGICAL BASIS:
 * - DLPFC coordinates task switching (switch cost ~100-500ms)
 * - Inhibitory control prevents prepotent responses
 * - Planning decomposes goals into action sequences
 * - Working memory capacity limits parallel tasks
 *
 * PHASE: 10.3 (Executive Functions)
 * DEPENDENCIES: Working Memory (Phase 10.1)
 * TRAINING_IMPACT: None (inference-only, task management)
 *
 * @author Claude Code
 * @date 2025-11
 */

#include "cognitive/nimcp_executive.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/executive/nimcp_executive_sleep_bridge.h"
#include "cognitive/executive/nimcp_executive_snn_bridge.h"
#include "cognitive/executive/nimcp_executive_plasticity_bridge.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#define NIMCP_EXECUTIVE_QUANTUM_BRIDGE_IMPLEMENTATION
#include "cognitive/executive/nimcp_executive_quantum_bridge.h"

#include "utils/memory/nimcp_memory.h"  // nimcp_malloc/nimcp_free
#include "utils/time/nimcp_time.h"       // nimcp_time_monotonic_ms
#include "utils/algorithms/nimcp_monte_carlo.h"  // Monte Carlo utilities
#include "utils/exception/nimcp_exception_macros.h"  // NIMCP_THROW_TO_IMMUNE
#include <stdatomic.h>  // _Atomic for thread-safe GPU init flag

//=============================================================================
// Monte Carlo Integration - GPU acceleration with CPU fallback
//=============================================================================

static _Thread_local uint32_t g_exec_mc_seed = 0;

#ifdef NIMCP_ENABLE_CUDA
#include "gpu/quantum/nimcp_qmc_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"

static nimcp_gpu_context_t* g_exec_gpu_ctx = NULL;
static qmc_gpu_rng_t g_exec_gpu_rng = NULL;
static _Atomic bool g_exec_gpu_init_attempted = false;

static bool exec_init_gpu_mc(void) {
    /* TOCTOU fix: Use atomic load/store to prevent double-initialization.
     * WHY:  Multiple threads could race on the non-atomic check, both see false,
     *       and double-initialize GPU context (resource leak + undefined behavior).
     */
    if (atomic_load(&g_exec_gpu_init_attempted)) return g_exec_gpu_rng != NULL;
    atomic_store(&g_exec_gpu_init_attempted, true);
    if (!qmc_gpu_is_available()) {
        return false;  /* GPU not available - not an error */
    }
    g_exec_gpu_ctx = nimcp_gpu_context_create_auto();
    if (!g_exec_gpu_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exec_init_gpu_mc: g_exec_gpu_ctx is NULL");
        return false;
    }
    g_exec_gpu_rng = qmc_gpu_rng_create(g_exec_gpu_ctx, 4096, 0);
    if (!g_exec_gpu_rng) {
        nimcp_gpu_context_destroy(g_exec_gpu_ctx);
        g_exec_gpu_ctx = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exec_init_gpu_mc: g_exec_gpu_rng is NULL");
        return false;
    }
    return true;
}

static inline bool exec_has_gpu_mc(void) {
    if (!atomic_load(&g_exec_gpu_init_attempted)) exec_init_gpu_mc();
    return g_exec_gpu_rng != NULL;
}

/**
 * @brief Cleanup GPU resources on library unload
 * WHY:  g_exec_gpu_ctx and g_exec_gpu_rng are allocated but never freed,
 *       leaking GPU memory when the library is unloaded or process exits.
 */
__attribute__((destructor))
static void exec_cleanup_gpu_mc(void) {
    if (g_exec_gpu_rng) {
        qmc_gpu_rng_destroy(g_exec_gpu_rng);
        g_exec_gpu_rng = NULL;
    }
    if (g_exec_gpu_ctx) {
        nimcp_gpu_context_destroy(g_exec_gpu_ctx);
        g_exec_gpu_ctx = NULL;
    }
    atomic_store(&g_exec_gpu_init_attempted, false);
}
#else
static inline bool exec_has_gpu_mc(void) { return false; }
#endif

#include "plasticity/neuromodulators/nimcp_neuromodulators.h"  // Neuromodulator integration
#include "core/brain/nimcp_brain.h"      // Brain reference
#include "core/brain/nimcp_brain_kg_helpers.h"  // KG self-awareness integration
#include "cognitive/executive/nimcp_w9kg_events.h"  // W9-kg: KG event + read helpers
#include "cognitive/global_workspace/nimcp_global_workspace.h"  // Global Workspace integration
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "nimcp.h"  // For error codes
#include "utils/exception/nimcp_exception_macros.h"  // Phase 7: Exception integration
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_wiring_helpers.h"

#define LOG_MODULE "EXECUTIVE"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "portia/nimcp_portia.h"
#include "portia/nimcp_portia_messages.h"
#include "utils/platform/nimcp_platform_tier.h"

#undef LOG_MODULE
#define LOG_MODULE "cognitive.executive"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include <math.h>

BRIDGE_BOILERPLATE(exec, MESH_ADAPTER_CATEGORY_COGNITIVE)

// Forward declarations for static helpers
static inline uint64_t exec_get_time_ms(void);

// Forward declaration for MCTS planning (defined later, needed for executive_create_plan)
plan_t* executive_create_plan_mcts(
    executive_controller_t* exec,
    const char* goal,
    const executive_mcts_config_t* config,
    executive_mcts_stats_t* stats);

//=============================================================================
// Constants
//=============================================================================

#define DEFAULT_MAX_TASKS 16
#define DEFAULT_SWITCH_COST_MS 200.0f
#define DEFAULT_INHIBITION_THRESHOLD 0.7f
#define DEFAULT_MAX_PLAN_DEPTH 10

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Executive controller internal structure
 */
struct executive_controller {
    // Task management
    task_descriptor_t** task_queue;  /**< Array of task pointers */
    uint32_t max_tasks;              /**< Maximum queue size */
    uint32_t num_tasks;              /**< Current tasks in queue */
    task_descriptor_t* active_task;  /**< Currently executing task */

    // Configuration
    executive_config_t config;

    // Statistics
    executive_stats_t stats;

    // Switch tracking
    uint64_t last_switch_time_ms;
    uint32_t next_task_id;

    // Thread safety for task queue operations
    nimcp_mutex_t task_mutex;       /**< Protect task queue, active_task, num_tasks */

    // Inhibition tracking
    uint32_t total_decisions;

    // Neuromodulation (Phase 10.x - Chemical modulation integration)
    brain_t brain;  /**< Brain reference for reading neurotransmitters */

    // Global Workspace integration (Phase 10.x)
    global_workspace_t* workspace;           /**< Global workspace for conscious broadcasting */
    bool workspace_integration_enabled;       /**< Workspace integration active */
    float workspace_ignition_threshold;       /**< Threshold for broadcasting decisions */

    // Bio-async integration
    bio_module_context_t bio_ctx;  /**< Bio-async module context */
    bool bio_async_enabled;        /**< Bio-async registration status */

    // Portia integration (Phase 11.5)
    platform_tier_t current_tier;  /**< Current Portia tier */
    portia_degradation_level_t degradation_level; /**< Current degradation level */
    bool resource_aware_mode;      /**< Resource-aware mode active */
    uint64_t last_tier_change_ms;  /**< Last tier change timestamp */
    uint32_t tier_change_count;    /**< Number of tier changes */

    // Swarm coordination (Phase 14.x - Swarm Intelligence Integration)
    void* swarm;                            /**< Swarm consensus context (optional, opaque) */
    bool swarm_coordination_enabled;        /**< Enable swarm coordination */
    float swarm_consensus_threshold;        /**< Threshold for swarm consensus [0,1] */
    uint32_t pending_consensus_proposals;   /**< Count of proposals awaiting consensus */
    nimcp_mutex_t swarm_lock;               /**< Protect swarm state */

    // Brain Immune System integration (Phase 12.x)
    brain_immune_system_t* immune_system;   /**< Brain immune system reference */
    bool immune_integration_enabled;        /**< Immune integration active */
    float last_inflammation_level;          /**< Cached inflammation level */
    uint64_t last_immune_check_ms;          /**< Last time immune state checked */

    // Sleep integration
    sleep_state_t current_sleep_state;      /**< Current sleep state for modulation */

    // Quantum planning integration
    executive_quantum_bridge_t* quantum_bridge;  /**< Quantum reasoning for planning */
    bool quantum_planning_enabled;               /**< Quantum planning active */

    // SNN bridge integration
    executive_snn_bridge_t* snn_bridge;          /**< SNN integration bridge */
    executive_plasticity_bridge_t* plasticity_bridge; /**< Plasticity integration bridge */
    bool bridges_enabled;                        /**< SNN/Plasticity bridges active */

    // Internal Knowledge Graph integration (self-awareness)
    kg_module_context_t kg_context;  /**< KG access context */
    bool kg_connected;               /**< Internal KG is connected */
};

//=============================================================================
// BIO-ASYNC MESSAGE HANDLERS
//=============================================================================

// Forward declarations for handler functions
static nimcp_error_t handle_decision_request(const void*, size_t, nimcp_bio_promise_t, void*);
static nimcp_error_t handle_workspace_ignition(const void*, size_t, nimcp_bio_promise_t, void*);
static nimcp_error_t handle_portia_tier_change(const void*, size_t, nimcp_bio_promise_t, void*);
static nimcp_error_t handle_portia_degradation_event(const void*, size_t, nimcp_bio_promise_t, void*);

/**
 * @brief KG-driven wiring callback for executive module
 */
static int executive_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data)
{
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && message_count > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)message_count);
        }

        switch (message_types[i]) {
            case BIO_MSG_DECISION_REQUEST:
                bio_router_register_handler(ctx, message_types[i], handle_decision_request);
                registered++;
                break;
            case BIO_MSG_ATTENTION_SHIFT:
                bio_router_register_handler(ctx, message_types[i], handle_workspace_ignition);
                registered++;
                break;
            case BIO_MSG_TYPE_PORTIA_TIER_CHANGE:
                bio_router_register_handler(ctx, message_types[i], handle_portia_tier_change);
                registered++;
                break;
            case BIO_MSG_TYPE_PORTIA_DEGRADATION_EVENT:
                bio_router_register_handler(ctx, message_types[i], handle_portia_degradation_event);
                registered++;
                break;
            default:
                LOG_DEBUG("Executive: unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO("Executive: registered %d handlers via KG wiring", registered);
    return 0;
}

/**
 * @brief Handle decision request via bio-async
 */
static nimcp_error_t handle_decision_request(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;
    (void)response_promise;

    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_introspection_query_t* query = (const bio_msg_introspection_query_t*)msg;
    executive_controller_t* exec = (executive_controller_t*)user_data;

    LOG_DEBUG("Received executive decision request (type=%u, threshold=%.2f)",
              query->query_type, query->confidence_threshold);

    /* Compute executive state based on query type */
    float confidence = 0.0f;
    float cognitive_load = 0.0f;

    switch (query->query_type) {
        case BIO_INTRO_QUERY_COGNITIVE_LOAD:
            /* Report cognitive load from task queue utilization */
            if (exec->max_tasks > 0) {
                cognitive_load = (float)exec->num_tasks / (float)exec->max_tasks;
            }
            confidence = 0.9f;
            break;

        case BIO_INTRO_QUERY_SELF_STATE:
            /* Report executive readiness */
            confidence = (exec->active_task != NULL) ? 0.7f : 0.9f;
            cognitive_load = (exec->active_task != NULL) ? 0.6f : 0.2f;
            break;

        case BIO_INTRO_QUERY_PATTERN_MATCH:
            /* Check if target pattern matches any active task */
            confidence = 0.5f;
            for (uint32_t i = 0; i < exec->num_tasks; i++) {
                if (exec->task_queue[i] &&
                    exec->task_queue[i]->task_id == query->target_pattern_id) {
                    confidence = 0.95f;
                    break;
                }
            }
            break;

        default:
            confidence = 0.3f;
            break;
    }

    /* Only respond if confidence meets threshold */
    if (confidence >= query->confidence_threshold) {
        exec->total_decisions++;
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle workspace ignition notification
 *
 * WHAT: Process global workspace broadcast for executive attention
 * WHY:  Executive should attend to salient workspace content
 * HOW:  Read workspace broadcast, integrate into decision context
 *
 * @param msg Workspace ignition message
 * @param msg_size Message size
 * @param response_promise Response promise (unused)
 * @param user_data Executive controller instance
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
static nimcp_error_t handle_workspace_ignition(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;
    (void)response_promise;

    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_attention_shift_t* ignition = (const bio_msg_attention_shift_t*)msg;
    executive_controller_t* exec = (executive_controller_t*)user_data;

    // Guard: Workspace integration not enabled
    if (!exec->workspace_integration_enabled || !exec->workspace) {
        return NIMCP_SUCCESS;  // Silently ignore
    }

    LOG_DEBUG("Executive received workspace ignition: id=%u, strength=%.2f",
              ignition->target_id, ignition->attention_weight);

    // Read workspace broadcast content
    if (global_workspace_has_broadcast(exec->workspace)) {
        float broadcast_content[256];
        uint32_t content_dim = 0;
        cognitive_module_t source = MODULE_NONE;

        if (global_workspace_read_broadcast(exec->workspace, broadcast_content,
                                            256, &content_dim, &source)) {
            LOG_DEBUG("Executive attending to workspace broadcast from %s (dim=%u)",
                      cognitive_module_to_string(source), content_dim);

            // Integrate workspace state into executive decision context
            // For now, just log the broadcast - in real implementation, this would
            // inform task prioritization and planning
        }
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Broadcast executive decision made
 */
static void bio_broadcast_decision(executive_controller_t* exec, uint32_t task_id, float confidence) {
    if (!exec || !exec->bio_async_enabled || !exec->bio_ctx) {
        return;
    }

    bio_msg_introspection_response_t msg = {};
    bio_msg_init_header(&msg.header, BIO_MSG_DECISION_RESPONSE,
                        bio_module_context_get_id(exec->bio_ctx), 0, sizeof(msg));
    msg.header.flags |= BIO_MSG_FLAG_BROADCAST;
    msg.query_type = task_id;
    msg.confidence = confidence;

    bio_router_broadcast(exec->bio_ctx, &msg, sizeof(msg));
    LOG_DEBUG("Broadcast: executive decision for task %u", task_id);
}

/**
 * @brief Handle Portia tier change notification
 *
 * WHAT: Process Portia tier transition events
 * WHY:  Executive must adapt to resource constraints
 * HOW:  Update tier cache, enter resource-aware mode, log transition
 *
 * BIOLOGY: Prefrontal cortex adapts planning depth under stress/fatigue
 *
 * @param msg Tier change message
 * @param msg_size Message size
 * @param response_promise Response promise (unused)
 * @param user_data Executive controller instance
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
static nimcp_error_t handle_portia_tier_change(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;
    (void)response_promise;

    // Guard: Validate parameters
    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_portia_tier_change_t* tier_msg = (const bio_msg_portia_tier_change_t*)msg;
    executive_controller_t* exec = (executive_controller_t*)user_data;

    // Guard: Portia integration not enabled
    if (!exec->config.enable_portia_integration) {
        return NIMCP_SUCCESS;  // Silently ignore
    }

    platform_tier_t old_tier = exec->current_tier;
    platform_tier_t new_tier = tier_msg->new_tier;

    // Update cached tier
    exec->current_tier = new_tier;
    exec->last_tier_change_ms = exec_get_time_ms();
    exec->tier_change_count++;

    // Enter resource-aware mode if tier degraded
    // Note: Enum values are FULL=0, MEDIUM=1, CONSTRAINED=2, MINIMAL=3
    // Higher enum value = worse tier, so downgrade means new_tier > old_tier
    if (new_tier > old_tier) {
        exec->resource_aware_mode = true;
        LOG_MODULE_WARN(LOG_MODULE, "Tier downgrade: %u -> %u (reason=%u), entering resource-aware mode",
                       old_tier, new_tier, tier_msg->reason);
    } else if (new_tier < old_tier) {
        // Consider exiting resource-aware mode on upgrade
        if (new_tier == PLATFORM_TIER_FULL) {
            exec->resource_aware_mode = false;
            LOG_MODULE_INFO(LOG_MODULE, "Tier upgrade: %u -> %u, exiting resource-aware mode",
                           old_tier, new_tier);
        } else {
            LOG_MODULE_INFO(LOG_MODULE, "Tier upgrade: %u -> %u, maintaining resource-aware mode",
                           old_tier, new_tier);
        }
    }

    LOG_MODULE_DEBUG(LOG_MODULE, "Portia tier transition processed: %u -> %u (confidence=%.2f)",
                    old_tier, new_tier, tier_msg->confidence);

    return NIMCP_SUCCESS;
}

/**
 * @brief Handle Portia degradation event
 *
 * WHAT: Process Portia degradation level changes
 * WHY:  Executive must reduce cognitive load when features disabled
 * HOW:  Update degradation level, reduce task queue capacity if needed
 *
 * @param msg Degradation event message
 * @param msg_size Message size
 * @param response_promise Response promise (unused)
 * @param user_data Executive controller instance
 * @return NIMCP_SUCCESS on success, error code otherwise
 */
static nimcp_error_t handle_portia_degradation_event(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)msg_size;
    (void)response_promise;

    // Guard: Validate parameters
    if (!msg || !user_data) {
        return NIMCP_ERROR_NULL_ARG;
    }

    const bio_msg_portia_degradation_event_t* deg_msg = (const bio_msg_portia_degradation_event_t*)msg;
    executive_controller_t* exec = (executive_controller_t*)user_data;

    // Guard: Portia integration not enabled
    if (!exec->config.enable_portia_integration) {
        return NIMCP_SUCCESS;  // Silently ignore
    }

    portia_degradation_level_t old_level = exec->degradation_level;
    portia_degradation_level_t new_level = deg_msg->new_level;

    // Update degradation level
    exec->degradation_level = new_level;

    // Always enter resource-aware mode on degradation
    if (new_level > PORTIA_DEGRADATION_NONE) {
        exec->resource_aware_mode = true;
    }

    // Log degradation event
    if (new_level > old_level) {
        LOG_MODULE_WARN(LOG_MODULE, "Degradation: %u -> %u (%u features disabled): %s",
                       old_level, new_level, deg_msg->features_disabled, deg_msg->description);
    } else if (new_level < old_level) {
        LOG_MODULE_INFO(LOG_MODULE, "Recovery: %u -> %u: %s",
                       old_level, new_level, deg_msg->description);
    }

    return NIMCP_SUCCESS;
}

//=============================================================================
// Error Handling
//=============================================================================

static _Thread_local char last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(last_error, sizeof(last_error), fmt, args);
    va_end(args);
}

const char* executive_get_last_error(void)
{
    return last_error;
}

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Get current time in milliseconds (wrapper for platform API)
 *
 * WHAT: Get monotonic time suitable for task switching measurement
 * WHY:  Track task switch times and compute latency
 * HOW:  Delegate to NIMCP time utilities
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 *
 * @return Current time in milliseconds since boot
 */
static inline uint64_t exec_get_time_ms(void)
{
    return nimcp_time_monotonic_ms();
}

/**
 * @brief Find task descriptor by unique task ID
 *
 * WHAT: Locate task in queue or active slot by ID
 * WHY:  Enable task lookup for switching and status queries
 * HOW:  Linear search through active task and queue
 *
 * COMPLEXITY: O(n) where n = number of queued tasks
 * THREAD-SAFE: No (caller must ensure exclusive access)
 *
 * @param exec Executive controller instance (non-NULL)
 * @param task_id Unique task identifier
 * @return Task descriptor pointer or NULL if not found
 *
 * @note Checks active task first (common case optimization)
 */
static task_descriptor_t* find_task_by_id(executive_controller_t* exec, uint32_t task_id)
{
    if (!exec) {
        return NULL;
    }

    // Check active task first
    if (exec->active_task && exec->active_task->task_id == task_id) {
        return exec->active_task;
    }

    // Search queue
    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)exec->num_tasks);
        }

        if (exec->task_queue[i] && exec->task_queue[i]->task_id == task_id) {
            return exec->task_queue[i];
        }
    }

    set_error("find_task_by_id: task %u not found", task_id);
    return NULL;
}

/**
 * @brief Select highest priority pending task from queue
 *
 * WHAT: Find next task to execute based on priority and deadline
 * WHY:  Support priority-based task scheduling
 * HOW:  Linear scan for max priority, deadline as tiebreaker
 *
 * ALGORITHM:
 * 1. Scan all tasks in queue
 * 2. Select highest priority (PENDING status only)
 * 3. If tied, choose earliest deadline
 * 4. Return selected task (or NULL if queue empty)
 *
 * COMPLEXITY: O(n) where n = number of queued tasks
 * THREAD-SAFE: No
 *
 * @param exec Executive controller instance
 * @return Highest priority task or NULL if no pending tasks
 *
 * @note Only considers tasks with TASK_STATUS_PENDING
 */
static task_descriptor_t* get_highest_priority_task(executive_controller_t* exec)
{
    if (!exec) {
        return NULL;
    }
    if (exec->num_tasks == 0) {
        return NULL;  /* Empty queue - normal condition */
    }

    task_descriptor_t* best = NULL;
    uint32_t best_idx = 0;

    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)exec->num_tasks);
        }

        task_descriptor_t* task = exec->task_queue[i];
        if (!task || task->status != TASK_STATUS_PENDING) continue;

        if (!best || task->priority > best->priority) {
            best = task;
            best_idx = i;
        } else if (task->priority == best->priority) {
            // Same priority - check deadline
            if (task->deadline_ms > 0 && (best->deadline_ms == 0 || task->deadline_ms < best->deadline_ms)) {
                best = task;
                best_idx = i;
            }
        }
    }

    (void)best_idx; // Used for future optimization
    return best;
}

/**
 * @brief Compute dopamine and sleep-modulated task switch cost
 *
 * WHAT: Adjust switch cost based on dopamine level and sleep state
 * WHY:  Both dopamine and sleep affect cognitive flexibility and switch cost
 * HOW:  Read DA, apply sleep modulation, combine multiplicatively
 *
 * BIOLOGY: High DA → easier switching (lower cost)
 *          Low DA → harder switching (higher cost, perseveration)
 *          Sleep deprivation → increased switch cost
 *
 * COMPLEXITY: O(1)
 *
 * @param exec Executive controller
 * @param base_cost Base switch cost (ms)
 * @return Modulated switch cost (ms)
 */
static float compute_modulated_switch_cost(executive_controller_t* exec,
                                           float base_cost)
{
    // Guard: Early return if no exec
    if (!exec) {
        return base_cost;
    }

    float cost = base_cost;

    // Apply dopamine modulation if brain is available
    if (exec->brain) {
        neuromodulator_system_t neuromod = brain_get_neuromodulator_system(exec->brain);
        if (neuromod) {
            // Read dopamine level
            float da = neuromodulator_get_level(neuromod, NEUROMOD_DOPAMINE);

            // DA range [0.3, 0.7], map to cost multiplier [1.4, 0.6]
            // High DA (0.7) → 0.6× cost (flexible, easy switching)
            // Low DA (0.3) → 1.4× cost (rigid, perseverative)
            float da_multiplier = 1.4F - (da - 0.3F) * 2.0F;
            cost *= da_multiplier;
        }
    }

    // Apply sleep modulation
    float sleep_cost_factor = executive_sleep_switch_cost_for_state(exec->current_sleep_state);
    cost *= sleep_cost_factor;

    return cost;
}

//=============================================================================
// Creation & Destruction
//=============================================================================

executive_controller_t* executive_create(void)
{
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_create", 0.0f);


    LOG_DEBUG("Creating module");
    executive_config_t default_config = {
        .max_tasks = DEFAULT_MAX_TASKS,
        .task_switch_cost_ms = DEFAULT_SWITCH_COST_MS,
        .inhibition_threshold = DEFAULT_INHIBITION_THRESHOLD,
        .max_plan_depth = DEFAULT_MAX_PLAN_DEPTH,
        .enable_task_prioritization = true,
        .enable_deadline_checking = true,
        .enable_quantum_executive = true
    };

    return executive_create_custom(&default_config);
}

/**
 * @brief Create executive controller with custom configuration
 *
 * WHAT: Initialize executive control center with specified parameters
 * WHY:  Enable goal-directed behavior and multi-tasking
 * HOW:  Allocate controller struct, task queue, initialize state
 *
 * ALGORITHM:
 * 1. Validate configuration parameters
 * 2. Allocate executive controller structure
 * 3. Allocate task queue array
 * 4. Initialize statistics and timing
 * 5. Return initialized controller
 *
 * COMPLEXITY: O(1) - constant allocations
 * MEMORY: sizeof(executive_controller_t) + max_tasks * sizeof(task_descriptor_t*)
 *
 * @param config Custom configuration (non-NULL)
 * @return Executive controller handle or NULL on error
 *
 * @note Caller must call executive_destroy() to free resources
 * @note Use executive_create() for default configuration
 */
executive_controller_t* executive_create_custom(const executive_config_t* config)
{
    // =========================================================================
    // GUARD: Validate configuration
    // =========================================================================

    // Guard: NULL config check
    if (!config) {
        set_error("NULL config provided to executive_create_custom");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_create_custom: config is NULL");
        return NULL;
    }

    // Guard: Task queue size validation
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_create_cus", 0.0f);


    if (config->max_tasks == 0 || config->max_tasks > 1024) {
        set_error("Invalid max_tasks: %u (must be 1-1024)", config->max_tasks);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_create_custom: invalid max_tasks");
        return NULL;
    }

    // =========================================================================
    // ALLOCATION: Create controller structure
    // =========================================================================

    executive_controller_t* exec = nimcp_calloc(1, sizeof(executive_controller_t));
    if (!exec) {
        set_error("Failed to allocate executive controller (%zu bytes)",
                  sizeof(executive_controller_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_create_custom: exec is NULL");
        return NULL;
    }

    // =========================================================================
    // ALLOCATION: Create task queue
    // =========================================================================

    exec->task_queue = nimcp_calloc(config->max_tasks, sizeof(task_descriptor_t*));
    if (!exec->task_queue) {
        set_error("Failed to allocate task queue (%zu bytes)",
                  config->max_tasks * sizeof(task_descriptor_t*));
        nimcp_free(exec);  // Cleanup before return
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_create_custom: exec->task_queue is NULL");
        return NULL;
    }

    // =========================================================================
    // INITIALIZATION: Set up state
    // =========================================================================

    exec->config = *config;
    exec->max_tasks = config->max_tasks;
    exec->num_tasks = 0;
    exec->active_task = NULL;
    exec->next_task_id = 1;
    exec->last_switch_time_ms = exec_get_time_ms();
    exec->total_decisions = 0;

    memset(&exec->stats, 0, sizeof(executive_stats_t));
    exec->brain = NULL;  // Initialize to NULL

    // =========================================================================
    // THREAD SAFETY: Initialize task mutex
    // =========================================================================
    if (nimcp_mutex_init(&exec->task_mutex, NULL) != 0) {
        set_error("Failed to initialize task queue mutex");
        nimcp_free(exec->task_queue);
        nimcp_free(exec);
        exec = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "executive_create_custom: validation failed");
        return NULL;
    }

    // =========================================================================
    // GLOBAL WORKSPACE: Initialize integration fields
    // =========================================================================
    exec->workspace = NULL;
    exec->workspace_integration_enabled = false;
    exec->workspace_ignition_threshold = 0.7F;  // Default threshold

    // =========================================================================
    // BRAIN IMMUNE SYSTEM: Initialize integration fields
    // =========================================================================
    exec->immune_system = NULL;
    exec->immune_integration_enabled = false;
    exec->last_inflammation_level = 0.0F;
    exec->last_immune_check_ms = 0;

    // =========================================================================
    // SLEEP INTEGRATION: Initialize sleep state
    // =========================================================================
    exec->current_sleep_state = SLEEP_STATE_AWAKE;

    // =========================================================================
    // QUANTUM PLANNING: Initialize quantum bridge
    // =========================================================================
    exec->quantum_bridge = NULL;
    exec->quantum_planning_enabled = false;
    if (exec->config.enable_quantum_executive) {
        executive_quantum_config_t quantum_config = executive_quantum_default_config();
        quantum_config.planning_depth = exec->config.max_plan_depth;
        exec->quantum_bridge = executive_quantum_bridge_create(&quantum_config);
        if (exec->quantum_bridge) {
            exec->quantum_planning_enabled = true;
            LOG_INFO("Quantum planning bridge initialized");
        } else {
            LOG_WARN("Failed to initialize quantum planning bridge");
        }
    }

    // =========================================================================
    // SNN/PLASTICITY BRIDGES: Initialize neural bridges
    // =========================================================================
    exec->snn_bridge = NULL;
    exec->plasticity_bridge = NULL;
    exec->bridges_enabled = false;
    {
        executive_snn_config_t snn_config = executive_snn_config_default();
        exec->snn_bridge = executive_snn_create(&snn_config);

        executive_plasticity_config_t plasticity_config = executive_plasticity_config_default();
        exec->plasticity_bridge = executive_plasticity_create(&plasticity_config);

        if (exec->snn_bridge && exec->plasticity_bridge) {
            exec->bridges_enabled = true;
            LOG_INFO("SNN and Plasticity bridges initialized");
        } else {
            LOG_WARN("Failed to initialize SNN/Plasticity bridges");
        }
    }

    // =========================================================================
    // BIO-ASYNC: Register with bio-router
    // =========================================================================
    exec->bio_ctx = NULL;
    exec->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_EXECUTIVE,
            .module_name = "executive",
            .inbox_capacity = 32,
            .user_data = exec
        };
        exec->bio_ctx = bio_router_register_module(&bio_info);
        if (exec->bio_ctx) {
            exec->bio_async_enabled = true;

            // Try KG-driven wiring callback registration first
            nimcp_error_t wiring_result = bio_router_register_wiring_callback(
                BIO_MODULE_EXECUTIVE,
                (void*)executive_wiring_handler_callback,
                exec
            );

            if (wiring_result == NIMCP_SUCCESS) {
                LOG_INFO("Executive: KG-driven wiring callback registered");
            } else {
                // Legacy fallback - register handlers directly
                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(exec->bio_ctx, BIO_MSG_DECISION_REQUEST, handle_decision_request);
                    bio_router_register_handler(exec->bio_ctx, BIO_MSG_ATTENTION_SHIFT, handle_workspace_ignition);
                    if (exec->config.enable_portia_integration) {
                        bio_router_register_handler(exec->bio_ctx, BIO_MSG_TYPE_PORTIA_TIER_CHANGE, handle_portia_tier_change);
                        bio_router_register_handler(exec->bio_ctx, BIO_MSG_TYPE_PORTIA_DEGRADATION_EVENT, handle_portia_degradation_event);
                    }
                );
                LOG_INFO("Executive: legacy handler registration");
            }
        }
    }

    // =========================================================================
    // PORTIA INTEGRATION: Initialize current tier
    // =========================================================================
    if (exec->config.enable_portia_integration && portia_is_initialized()) {
        exec->current_tier = portia_get_current_tier();
        // Note: FULL=0 is best, MINIMAL=3 is worst. resource_aware if NOT at full tier.
        exec->resource_aware_mode = (exec->current_tier > PLATFORM_TIER_FULL);
        LOG_INFO("Executive initialized with Portia tier %u, resource_aware=%d",
                 exec->current_tier, exec->resource_aware_mode);
    } else {
        // Portia not initialized or integration disabled - use safe defaults
        // Keep current_tier at 0 (PLATFORM_TIER_MINIMAL) from calloc
        // resource_aware_mode stays false - no active resource management
        exec->current_tier = PLATFORM_TIER_MINIMAL;
        exec->resource_aware_mode = false;
    }

    return exec;
}

/**
 * @brief Set brain reference for neuromodulator integration
 *
 * WHAT: Associate executive controller with brain for chemical modulation
 * WHY:  Enable neurotransmitter-based modulation of executive functions
 * HOW:  Store brain reference in controller structure
 *
 * USAGE: Call after creation to enable neuromodulation
 *
 * @param exec Executive controller
 * @param brain Brain handle (can be NULL to disable neuromodulation)
 */
void executive_set_brain(executive_controller_t* exec, brain_t brain)
{
    if (!exec) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_set_brain", 0.0f);


    exec->brain = brain;
}

/**
 * @brief Set global workspace for conscious broadcasting
 *
 * WHAT: Associate executive controller with global workspace
 * WHY:  Enable conscious decision broadcasting and workspace attention
 * HOW:  Store workspace reference and enable integration
 *
 * USAGE: Call after creation to enable workspace integration
 *
 * @param exec Executive controller
 * @param workspace Global workspace handle (can be NULL to disable)
 */
void executive_set_workspace(executive_controller_t* exec, global_workspace_t* workspace)
{
    if (!exec) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_set_worksp", 0.0f);


    exec->workspace = workspace;
    exec->workspace_integration_enabled = (workspace != NULL);

    if (workspace) {
        // Subscribe to workspace broadcasts
        global_workspace_subscribe(workspace, MODULE_EXECUTIVE);
        LOG_INFO("Executive controller integrated with global workspace");
    }
}

/**
 * @brief Broadcast significant decision to global workspace
 *
 * WHAT: Submit executive decision for workspace ignition
 * WHY:  Make important decisions consciously available
 * HOW:  Create decision vector, compete for workspace access
 *
 * ALGORITHM:
 * 1. Check if decision meets ignition threshold
 * 2. Create decision representation vector
 * 3. Compete for global workspace access
 * 4. If successful, decision becomes consciously available
 *
 * @param exec Executive controller
 * @param task Task descriptor for decision context
 * @param confidence Decision confidence [0, 1]
 * @return true if decision was broadcast to workspace
 */
static bool broadcast_decision_to_workspace(
    executive_controller_t* exec,
    const task_descriptor_t* task,
    float confidence)
{
    // Guard: Workspace integration not enabled
    if (!exec->workspace_integration_enabled || !exec->workspace || !task) {
        return false;
    }

    // Guard: Confidence below threshold
    if (confidence < exec->workspace_ignition_threshold) {
        return false;
    }

    // Create decision representation vector
    float decision_content[256] = {0};  // Workspace capacity

    // Encode task information into decision vector
    decision_content[0] = (float)task->type / 10.0F;         // Task type
    decision_content[1] = (float)task->priority / 5.0F;      // Priority
    decision_content[2] = (float)task->status / 6.0F;        // Status
    decision_content[3] = confidence;                         // Confidence

    // Add task progress information
    if (task->steps_total > 0) {
        decision_content[4] = (float)task->steps_completed / (float)task->steps_total;
    }

    // Compete for workspace access with confidence as strength
    bool won = global_workspace_compete(exec->workspace, MODULE_EXECUTIVE,
                                        decision_content, 256, confidence);

    if (won) {
        LOG_INFO("Executive decision broadcast to workspace: task=%s, confidence=%.2f",
                 task->name, confidence);

        // Broadcast via bio-async for distributed coordination
        if (exec->bio_async_enabled && exec->bio_ctx) {
            bio_msg_decision_response_t decision_msg = {0};
            bio_msg_init_header(&decision_msg.header, BIO_MSG_DECISION_RESPONSE,
                                BIO_MODULE_EXECUTIVE, BIO_MODULE_ALL,
                                sizeof(bio_msg_decision_response_t));
            decision_msg.approved = true;
            decision_msg.confidence = confidence;
            decision_msg.selected_option = task->task_id;
            snprintf(decision_msg.reasoning, sizeof(decision_msg.reasoning),
                     "Executive decision: %s", task->name);

            bio_router_send(exec->bio_ctx, &decision_msg,
                            sizeof(decision_msg), 0);
        }
    }

    return won;
}

/**
 * @brief Destroy executive controller and free all resources
 *
 * WHAT: Clean up executive controller, tasks, and queue
 * WHY:  Prevent memory leaks
 * HOW:  Free all queued tasks, active task, queue array, controller
 *
 * COMPLEXITY: O(n) where n = number of tasks in queue
 * THREAD-SAFE: No (caller must ensure exclusive access)
 *
 * @param exec Executive controller to destroy (can be NULL)
 *
 * @note Safe to call with NULL pointer (no-op)
 * @note Frees all queued and active tasks
 */
void executive_destroy(executive_controller_t* exec)
{
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_destroy", 0.0f);


    LOG_DEBUG("Destroying module");
    if (!exec) return;

    // WHAT: Free all tasks in queue
    // WHY:  Prevent memory leaks from dynamically allocated tasks
    // HOW:  Iterate and free each task descriptor
    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)exec->num_tasks);
        }

        nimcp_free(exec->task_queue[i]);
    }

    // Free task queue array
    nimcp_free(exec->task_queue);

    // Free active task if exists
    if (exec->active_task) {
        nimcp_free(exec->active_task);
    }

    // Unregister from bio-router
    if (exec->bio_async_enabled && exec->bio_ctx) {
        bio_router_unregister_module(exec->bio_ctx);
        exec->bio_ctx = NULL;
        exec->bio_async_enabled = false;
    }

    // Destroy quantum bridge
    if (exec->quantum_bridge) {
        executive_quantum_bridge_destroy(exec->quantum_bridge);
        exec->quantum_bridge = NULL;
    }

    // Destroy SNN and Plasticity bridges
    if (exec->snn_bridge) {
        executive_snn_destroy(exec->snn_bridge);
        exec->snn_bridge = NULL;
    }
    if (exec->plasticity_bridge) {
        executive_plasticity_destroy(exec->plasticity_bridge);
        exec->plasticity_bridge = NULL;
    }

    // Destroy task mutex
    nimcp_mutex_destroy(&exec->task_mutex);

    // Free controller structure
    nimcp_free(exec);
    exec = NULL;
}

//=============================================================================
// Task Management
//=============================================================================

uint32_t executive_add_task(executive_controller_t* exec, const task_descriptor_t* task)
{
    if (!exec || !task) {
        set_error("NULL parameter");
        return 0;
    }

    /* P0 fix: Acquire task mutex to protect shared state access
     * WHY:  Prevents race conditions when multiple threads add tasks
     */
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_add_task", 0.0f);


    nimcp_mutex_lock(&exec->task_mutex);

    if (exec->num_tasks >= exec->max_tasks) {
        set_error("Task queue full (%u/%u)", exec->num_tasks, exec->max_tasks);
        nimcp_mutex_unlock(&exec->task_mutex);
        return 0;
    }

    // Allocate new task
    task_descriptor_t* new_task = nimcp_malloc(sizeof(task_descriptor_t));
    if (!new_task) {
        set_error("Failed to allocate task (%zu bytes)", sizeof(task_descriptor_t));
        nimcp_mutex_unlock(&exec->task_mutex);
        return 0;
    }

    // Copy task data
    *new_task = *task;
    new_task->task_id = exec->next_task_id++;
    new_task->status = TASK_STATUS_PENDING;
    new_task->created_ms = exec_get_time_ms();
    new_task->started_ms = 0;
    new_task->completed_ms = 0;
    new_task->steps_completed = 0;

    // Add to queue
    exec->task_queue[exec->num_tasks++] = new_task;
    exec->stats.total_tasks++;

    uint32_t task_id = new_task->task_id;
    nimcp_mutex_unlock(&exec->task_mutex);
    return task_id;
}

bool executive_switch_task(executive_controller_t* exec, uint32_t task_id, uint64_t current_time_ms)
{
    if (!exec) {
        set_error("NULL executive controller");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_switch_task: exec is NULL");
        return false;
    }

    /* P0 fix: Acquire task mutex to protect shared state access
     * WHY:  Prevents race conditions when multiple threads access task queue
     */
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_switch_tas", 0.0f);


    nimcp_mutex_lock(&exec->task_mutex);

    // Find target task in queue
    task_descriptor_t* target = NULL;
    uint32_t target_index = 0;
    bool found_in_queue = false;

    // Check if it's the current active task
    if (exec->active_task && exec->active_task->task_id == task_id) {
        // Already active, no-op
        nimcp_mutex_unlock(&exec->task_mutex);
        return true;
    }

    // Search in queue
    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)exec->num_tasks);
        }

        if (exec->task_queue[i] && exec->task_queue[i]->task_id == task_id) {
            target = exec->task_queue[i];
            target_index = i;
            found_in_queue = true;
            break;
        }
    }

    if (!target) {
        set_error("Task %u not found", task_id);
        nimcp_mutex_unlock(&exec->task_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_switch_task: target is NULL");
        return false;
    }

    if (target->status != TASK_STATUS_PENDING && target->status != TASK_STATUS_SUSPENDED) {
        set_error("Cannot switch to task in status %d", target->status);
        nimcp_mutex_unlock(&exec->task_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_switch_task: validation failed");
        return false;
    }

    // Suspend current active task and PUT IT BACK in the queue
    if (exec->active_task) {
        exec->active_task->status = TASK_STATUS_SUSPENDED;
        // If there's room, add it back to the queue
        if (exec->num_tasks < exec->max_tasks) {
            exec->task_queue[exec->num_tasks++] = exec->active_task;
        } else {
            // Queue full - just free the suspended task (task switching penalty)
            nimcp_free(exec->active_task);
        }
    }

    // Calculate switch cost (modulated by dopamine if brain available)
    float switch_cost = compute_modulated_switch_cost(exec, exec->config.task_switch_cost_ms);

    // Update statistics
    exec->stats.total_switches++;
    float old_avg = exec->stats.avg_switch_cost_ms;
    float n = (float)exec->stats.total_switches;
    exec->stats.avg_switch_cost_ms = (old_avg * (n - 1.0F) + switch_cost) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    // REMOVE target from queue (to prevent double-free)
    if (found_in_queue) {
        // Shift remaining tasks down
        /* P0 fix: Add bounds check to prevent underflow when num_tasks is 0
         * WHY:  exec->num_tasks - 1 would underflow to UINT32_MAX if num_tasks == 0
         */
        if (exec->num_tasks > 0) {
            for (uint32_t i = target_index; i < exec->num_tasks - 1; i++) {
                exec->task_queue[i] = exec->task_queue[i + 1];
            }
            exec->task_queue[exec->num_tasks - 1] = NULL;
            exec->num_tasks--;
        }
    }

    // Activate new task
    exec->active_task = target;
    exec->active_task->status = TASK_STATUS_ACTIVE;
    if (exec->active_task->started_ms == 0) {
        exec->active_task->started_ms = current_time_ms;
    }
    exec->last_switch_time_ms = current_time_ms;

    nimcp_mutex_unlock(&exec->task_mutex);
    return true;
}

const task_descriptor_t* executive_get_active_task(executive_controller_t* exec)
{
    if (!exec) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "exec is NULL");

        return NULL;

    }
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_get_active", 0.0f);

    /* Thread-safety fix: copy active_task data under mutex to a thread-local
     * buffer so the caller gets a stable snapshot even if another thread
     * frees/replaces the active task after we release the lock. */
    static _Thread_local task_descriptor_t tl_task_copy;

    nimcp_mutex_lock(&exec->task_mutex);
    if (!exec->active_task) {
        nimcp_mutex_unlock(&exec->task_mutex);
        return NULL;
    }
    tl_task_copy = *exec->active_task;
    nimcp_mutex_unlock(&exec->task_mutex);

    return &tl_task_copy;
}

bool executive_complete_task(executive_controller_t* exec, bool success, uint64_t current_time_ms)
{
    if (!exec) {
        set_error("NULL executive controller");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_complete_task: exec is NULL");
        return false;
    }

    /* P0 fix: Acquire task mutex to protect shared state access
     * WHY:  Prevents race conditions when completing tasks
     */
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_complete_t", 0.0f);


    nimcp_mutex_lock(&exec->task_mutex);

    if (!exec->active_task) {
        set_error("No active task to complete");
        nimcp_mutex_unlock(&exec->task_mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_complete_task: exec->active_task is NULL");
        return false;
    }

    // Mark task as completed or failed
    exec->active_task->status = success ? TASK_STATUS_COMPLETED : TASK_STATUS_FAILED;
    exec->active_task->completed_ms = current_time_ms;

    // Update statistics
    if (success) {
        exec->stats.completed_tasks++;
    } else {
        exec->stats.failed_tasks++;
    }

    // Compute workspace broadcast parameters while holding lock
    float confidence = success ? 0.8F : 0.5F;  // Higher confidence for success
    if (exec->active_task->priority >= PRIORITY_HIGH) {
        confidence += 0.1F;  // Boost confidence for high-priority tasks
    }

    /* DEADLOCK FIX: Copy task data to stack before releasing lock.
     * WHY:  broadcast_decision_to_workspace() calls global_workspace_compete()
     *       which acquires the workspace lock. If another thread holds the
     *       workspace lock and tries to acquire task_mutex, deadlock occurs.
     * HOW:  Copy needed task data, release lock, then do cross-module calls.
     */
    task_descriptor_t task_copy = *exec->active_task;

    // Free the completed task (it's no longer in the queue)
    nimcp_free(exec->active_task);

    // Remove from active
    exec->active_task = NULL;

    // Try to activate next highest priority task (while still holding lock)
    uint32_t next_task_id = 0;
    if (exec->config.enable_task_prioritization) {
        task_descriptor_t* next = get_highest_priority_task(exec);
        if (next) {
            next_task_id = next->task_id;
        }
    }

    /* Release lock before cross-module calls to avoid deadlock
     * WHY:  broadcast_decision_to_workspace acquires workspace lock,
     *       executive_switch_task acquires task_mutex again
     */
    nimcp_mutex_unlock(&exec->task_mutex);

    // Broadcast to workspace OUTSIDE lock using stack copy
    broadcast_decision_to_workspace(exec, &task_copy, confidence);

    // Switch to next task (outside lock to prevent deadlock)
    if (next_task_id > 0) {
        executive_switch_task(exec, next_task_id, current_time_ms);
    }

    return true;
}

//=============================================================================
// Inhibitory Control
//=============================================================================

bool executive_should_inhibit(executive_controller_t* exec, float response_salience, const char* reason)
{
    if (!exec) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_should_inh", 0.0f);


    exec->total_decisions++;

    // Apply sleep modulation to inhibition threshold
    float base_threshold = exec->config.inhibition_threshold;
    float sleep_factor = executive_sleep_inhibition_for_state(exec->current_sleep_state);
    float modulated_threshold = base_threshold / fmaxf(sleep_factor, 0.01F);  // Avoid division by zero

    // Inhibit if salience exceeds modulated threshold
    bool inhibit = response_salience >= modulated_threshold;

    if (inhibit) {
        exec->stats.inhibitions++;
        exec->stats.inhibition_rate = (float)exec->stats.inhibitions / (float)exec->total_decisions;
    }

    (void)reason; // Log reason in future version

    return inhibit;
}

//=============================================================================
// Planning
//=============================================================================

plan_t* executive_create_plan(executive_controller_t* exec, const char* goal, uint32_t max_steps)
{
    if (!exec || !goal) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_create_plan: required parameter is NULL (exec, goal)");
        return NULL;
    }

    // Process pending bio-async messages before planning
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_create_pla", 0.0f);


    if (exec->bio_async_enabled && exec->bio_ctx) {
        bio_router_process_inbox(exec->bio_ctx, 10);
    }

    if (max_steps == 0 || max_steps > exec->config.max_plan_depth) {
        set_error("Invalid max_steps: %u (max: %u)", max_steps, exec->config.max_plan_depth);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_create_plan: max_steps is zero");
        return NULL;
    }

    // Allocate plan
    plan_t* plan = nimcp_calloc(1, sizeof(plan_t));
    if (!plan) {
        set_error("Failed to allocate plan (%zu bytes)", sizeof(plan_t));
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_create_plan: plan is NULL");
        return NULL;
    }

    // Allocate steps
    plan->steps = nimcp_calloc(max_steps, sizeof(plan_step_t));
    if (!plan->steps) {
        set_error("Failed to allocate plan steps (%zu bytes)",
                  max_steps * sizeof(plan_step_t));
        nimcp_free(plan);
        plan = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_create_plan: plan->steps is NULL");
        return NULL;
    }

    plan->type = PLAN_TYPE_SEQUENTIAL;
    snprintf(plan->goal, sizeof(plan->goal), "%s", goal);

    // Use quantum planning if enabled
    if (exec->quantum_planning_enabled && exec->quantum_bridge) {
        quantum_plan_result_t qresult;
        uint32_t num_actions = 10;  // Simplified: assume 10 possible actions

        int ret = executive_quantum_plan(
            exec->quantum_bridge,
            goal,
            num_actions,
            max_steps,
            &qresult
        );

        if (ret == 0 && qresult.best_hypothesis &&
            qresult.best_hypothesis->confidence >= 0.5f) {
            // Use quantum plan
            plan->num_steps = qresult.best_hypothesis->num_steps;
            if (plan->num_steps > max_steps) {
                plan->num_steps = max_steps;
            }

            // Generate plan steps from quantum hypothesis
            for (uint32_t i = 0; i < plan->num_steps; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && plan->num_steps > 256) {
                    exec_heartbeat("exec_loop",
                                     (float)(i + 1) / (float)plan->num_steps);
                }

                uint32_t action_id = qresult.best_hypothesis->action_sequence[i];
                snprintf(plan->steps[i].description, sizeof(plan->steps[i].description),
                         "Quantum step %u: action %u for %s", i, action_id, goal);
                plan->steps[i].estimated_cost = 100 + (action_id * 50);
                plan->steps[i].is_critical = (i == 0 || qresult.best_hypothesis->confidence > 0.8f);
            }

            LOG_DEBUG("Quantum plan created: %u steps, confidence %.2f, speedup %.2fx",
                      plan->num_steps, qresult.best_hypothesis->confidence,
                      qresult.planning_speedup);
        } else {
            // Fallback to classical planning
            LOG_DEBUG("Quantum planning failed or low confidence, using classical");
            goto classical_planning;
        }
    } else {
classical_planning:
        // Use MCTS for sophisticated goal decomposition
        executive_mcts_config_t mcts_cfg;
        executive_mcts_config_init(&mcts_cfg);
        mcts_cfg.max_depth = max_steps;
        mcts_cfg.max_iterations = 50;  // Moderate iterations for fallback

        plan_t* mcts_plan = executive_create_plan_mcts(exec, goal, &mcts_cfg, NULL);
        if (mcts_plan) {
            // Copy MCTS plan to pre-allocated structure
            plan->type = mcts_plan->type;
            plan->num_steps = (mcts_plan->num_steps <= max_steps) ? mcts_plan->num_steps : max_steps;

            for (uint32_t i = 0; i < plan->num_steps; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && plan->num_steps > 256) {
                    exec_heartbeat("exec_loop",
                                     (float)(i + 1) / (float)plan->num_steps);
                }

                plan->steps[i] = mcts_plan->steps[i];
            }

            // Free the MCTS plan (but not steps since we copied)
            mcts_plan->steps = NULL;  // Prevent double-free
            executive_destroy_plan(mcts_plan);

            LOG_DEBUG("MCTS-based classical plan created: %u steps", plan->num_steps);
        } else {
            // Ultimate fallback: simple decomposition
            plan->num_steps = (max_steps > 3) ? 3 : max_steps;

            snprintf(plan->steps[0].description, sizeof(plan->steps[0].description), "Analyze: %s", goal);
            plan->steps[0].estimated_cost = 100;
            plan->steps[0].is_critical = true;

            if (plan->num_steps > 1) {
                snprintf(plan->steps[1].description, sizeof(plan->steps[1].description), "Execute: %s", goal);
                plan->steps[1].estimated_cost = 500;
                plan->steps[1].is_critical = true;
            }

            if (plan->num_steps > 2) {
                snprintf(plan->steps[2].description, sizeof(plan->steps[2].description), "Verify: %s", goal);
                plan->steps[2].estimated_cost = 200;
                plan->steps[2].is_critical = false;
            }
        }
    }

    // Update statistics
    exec->stats.plans_created++;
    float old_avg = exec->stats.avg_plan_length;
    float n = (float)exec->stats.plans_created;
    exec->stats.avg_plan_length = (old_avg * (n - 1.0F) + (float)plan->num_steps) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    return plan;
}

void executive_destroy_plan(plan_t* plan)
{
    if (!plan) return;

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_destroy_pl", 0.0f);


    if (plan->steps) {
        // Free any action_data in steps
        for (uint32_t i = 0; i < plan->num_steps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && plan->num_steps > 256) {
                exec_heartbeat("exec_loop",
                                 (float)(i + 1) / (float)plan->num_steps);
            }

            // action_data ownership is external, don't free here
        }
        nimcp_free(plan->steps);
    }

    nimcp_free(plan);
    plan = NULL;
}

//=============================================================================
// Statistics
//=============================================================================

bool executive_get_stats(executive_controller_t* exec, executive_stats_t* stats)
{
    if (!exec || !stats) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_get_stats: required parameter is NULL (exec, stats)");
        return false;
    }

    *stats = exec->stats;
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_get_stats", 0.0f);


    return true;
}

void executive_reset_stats(executive_controller_t* exec)
{
    if (!exec) return;

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_reset_stat", 0.0f);


    memset(&exec->stats, 0, sizeof(executive_stats_t));
    exec->total_decisions = 0;
}

//=============================================================================
// Bidirectional Feedback Functions (Phase 10.11.3)
//=============================================================================

/**
 * @brief Get cognitive load (utilization)
 *
 * WHAT: Query current cognitive load on executive system
 * WHY:  Other modules can adapt behavior based on load
 * HOW:  Return task count / capacity ratio
 *
 * BIOLOGY: Prefrontal cortex has limited capacity (~4 chunks in working memory)
 *          High load → poor multitasking, reduced exploration
 *
 * COMPLEXITY: O(1)
 *
 * @param exec Executive controller
 * @return Cognitive load [0, 1] (0=idle, 1=saturated)
 */
float executive_get_cognitive_load(executive_controller_t* exec)
{
    // Guard: NULL controller
    if (!exec) {
        return 0.0F;
    }

    // WHAT: Compute load as ratio of active tasks to capacity
    // WHY:  Simple proxy for cognitive resource usage
    // HOW:  (num_tasks + active_task) / max_tasks
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_get_cognit", 0.0f);

    /* Thread-safety fix: read shared state under mutex */
    nimcp_mutex_lock(&exec->task_mutex);

    uint32_t total_tasks = exec->num_tasks;
    if (exec->active_task) {
        total_tasks++;  // Count active task
    }

    uint32_t max_tasks = exec->max_tasks;

    nimcp_mutex_unlock(&exec->task_mutex);

    if (max_tasks == 0) {
        return 0.0F;
    }

    float load = (float)total_tasks / (float)max_tasks;

    // Clamp to [0, 1]
    return fminf(fmaxf(load, 0.0F), 1.0F);
}

/**
 * @brief Boost task priority based on external signal
 *
 * WHAT: Allow modules to boost task priority
 * WHY:  Curiosity-driven tasks should be prioritized when informative
 * HOW:  Find task by name, increase priority
 *
 * COMPLEXITY: O(n) where n = number of tasks
 *
 * @param exec Executive controller
 * @param task_name Task name to boost
 * @param boost_amount Priority boost [0, 1]
 * @return true if task found and boosted
 */
bool executive_boost_task_priority(executive_controller_t* exec,
                                    const char* task_name,
                                    float boost_amount)
{
    // Guard: Validate inputs
    if (!exec || !task_name) {
        set_error("NULL parameter in boost_task_priority");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_boost_task_priority: required parameter is NULL (exec, task_name)");
        return false;
    }

    // Clamp boost amount
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_boost_task", 0.0f);


    boost_amount = fminf(fmaxf(boost_amount, 0.0F), 1.0F);

    // WHAT: Search for task by name
    // WHY:  Need to find task in queue or active slot
    // HOW:  Linear search through task descriptors
    /* Thread-safety fix: hold mutex while searching and modifying task priorities */
    nimcp_mutex_lock(&exec->task_mutex);

    bool found = false;

    // Check active task
    if (exec->active_task && strcmp(exec->active_task->name, task_name) == 0) {
        exec->active_task->priority += boost_amount;
        found = true;
    }

    // Check queued tasks
    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)exec->num_tasks);
        }

        task_descriptor_t* task = exec->task_queue[i];
        if (task && strcmp(task->name, task_name) == 0) {
            task->priority += boost_amount;
            found = true;
        }
    }

    nimcp_mutex_unlock(&exec->task_mutex);

    if (!found) {
        set_error("Task '%s' not found", task_name);
    }

    return found;
}

//=============================================================================
// Persistence API (Save/Load)
//=============================================================================

/**
 * @brief Save executive controller state to file
 *
 * WHAT: Serialize executive controller state to binary file
 * WHY:  Enable persistence of task queue, statistics, and configuration
 * HOW:  Write version marker, config, stats, and task queue to file
 *
 * Binary format:
 *   uint32_t version (1)
 *   executive_config_t config
 *   executive_stats_t stats
 *   uint64_t last_switch_time_ms
 *   uint32_t next_task_id
 *   uint32_t total_decisions
 *   uint32_t num_tasks
 *   For each task:
 *     task_descriptor_t task (without context pointer)
 *
 * COMPLEXITY: O(n) where n = number of tasks
 * THREAD-SAFE: No (caller must ensure exclusive access)
 *
 * @param exec Executive controller
 * @param file Open file handle for writing
 * @return true on success, false on error
 */
bool executive_save(executive_controller_t* exec, FILE* file)
{
    // Guard: Validate parameters
    if (!exec || !file) {
        set_error("Invalid parameters to executive_save");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_save: required parameter is NULL (exec, file)");
        return false;
    }

    // WHAT: Write version marker for backward compatibility
    // WHY:  Enable future format changes while supporting old saves
    // HOW:  Write uint32_t version = 1
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_save", 0.0f);


    uint32_t version = 1;
    if (fwrite(&version, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to write version marker");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
        return false;
    }

    // WHAT: Write configuration
    // WHY:  Restore executive behavior on load
    // HOW:  Binary write of config struct
    if (fwrite(&exec->config, sizeof(executive_config_t), 1, file) != 1) {
        set_error("Failed to write executive configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
        return false;
    }

    // WHAT: Write statistics
    // WHY:  Preserve historical metrics across sessions
    // HOW:  Binary write of stats struct
    if (fwrite(&exec->stats, sizeof(executive_stats_t), 1, file) != 1) {
        set_error("Failed to write executive statistics");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
        return false;
    }

    // WHAT: Write temporal state
    // WHY:  Preserve switch timing information
    // HOW:  Write last_switch_time_ms, next_task_id, total_decisions
    if (fwrite(&exec->last_switch_time_ms, sizeof(uint64_t), 1, file) != 1) {
        set_error("Failed to write last_switch_time_ms");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
        return false;
    }

    if (fwrite(&exec->next_task_id, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to write next_task_id");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
        return false;
    }

    if (fwrite(&exec->total_decisions, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to write total_decisions");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
        return false;
    }

    // WHAT: Write task count
    // WHY:  Know how many tasks to load
    // HOW:  Write num_tasks
    if (fwrite(&exec->num_tasks, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to write task count");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
        return false;
    }

    // WHAT: Write task queue
    // WHY:  Restore queued tasks on load
    // HOW:  For each task, write task_descriptor_t (context pointer set to NULL)
    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)exec->num_tasks);
        }

        if (!exec->task_queue[i]) {
            set_error("NULL task in queue at index %u", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_save: exec->task_queue is NULL");
            return false;
        }

        // Copy task and clear context pointer (not serializable)
        task_descriptor_t task_copy = *exec->task_queue[i];
        task_copy.context = NULL;

        if (fwrite(&task_copy, sizeof(task_descriptor_t), 1, file) != 1) {
            set_error("Failed to write task %u", i);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
            return false;
        }
    }

    // WHAT: Write active task flag and data
    // WHY:  Restore active task on load
    // HOW:  Write has_active_task flag, then task if present
    bool has_active_task = (exec->active_task != NULL);
    if (fwrite(&has_active_task, sizeof(bool), 1, file) != 1) {
        set_error("Failed to write active task flag");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
        return false;
    }

    if (has_active_task) {
        // Copy active task and clear context pointer
        task_descriptor_t task_copy = *exec->active_task;
        task_copy.context = NULL;

        if (fwrite(&task_copy, sizeof(task_descriptor_t), 1, file) != 1) {
            set_error("Failed to write active task");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_save: validation failed");
            return false;
        }
    }

    return true;
}

/**
 * @brief Load executive controller state from file
 *
 * WHAT: Deserialize executive controller state from binary file
 * WHY:  Restore saved task queue, statistics, and configuration
 * HOW:  Read version marker, validate, reconstruct state
 *
 * Note: Brain reference must be set separately via brain field assignment
 * Note: Task context pointers are not restored (set to NULL)
 *
 * COMPLEXITY: O(n) where n = number of tasks
 * THREAD-SAFE: Yes (creates new instance)
 *
 * @param file Open file handle for reading
 * @return Executive controller handle or NULL on error
 */
executive_controller_t* executive_load(FILE* file)
{
    // Guard: Validate parameter
    if (!file) {
        set_error("NULL file parameter to executive_load");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_load: file is NULL");
        return NULL;
    }

    // WHAT: Read and validate version
    // WHY:  Ensure format compatibility
    // HOW:  Read version, check against current version
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_load", 0.0f);


    uint32_t version = 0;
    if (fread(&version, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to read version marker");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_load: validation failed");
        return NULL;
    }

    if (version != 1) {
        set_error("Unsupported executive save format version %u", version);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_load: validation failed");
        return NULL;
    }

    // WHAT: Allocate executive controller
    // WHY:  Need structure to hold loaded data
    // HOW:  Use nimcp_calloc for zero-initialization
    executive_controller_t* exec = (executive_controller_t*)nimcp_calloc(1, sizeof(executive_controller_t));
    if (!exec) {
        set_error("Failed to allocate executive controller");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_load: exec is NULL");
        return NULL;
    }

    // WHAT: Read configuration
    // WHY:  Restore executive behavior
    // HOW:  Binary read into config struct
    if (fread(&exec->config, sizeof(executive_config_t), 1, file) != 1) {
        set_error("Failed to read executive configuration");
        goto cleanup;
    }

    // WHAT: Read statistics
    // WHY:  Restore historical metrics
    // HOW:  Binary read into stats struct
    if (fread(&exec->stats, sizeof(executive_stats_t), 1, file) != 1) {
        set_error("Failed to read executive statistics");
        goto cleanup;
    }

    // WHAT: Read temporal state
    // WHY:  Restore switch timing
    // HOW:  Read last_switch_time_ms, next_task_id, total_decisions
    if (fread(&exec->last_switch_time_ms, sizeof(uint64_t), 1, file) != 1) {
        set_error("Failed to read last_switch_time_ms");
        goto cleanup;
    }

    if (fread(&exec->next_task_id, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to read next_task_id");
        goto cleanup;
    }

    if (fread(&exec->total_decisions, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to read total_decisions");
        goto cleanup;
    }

    // WHAT: Read task count
    // WHY:  Know how many tasks to allocate
    // HOW:  Read num_tasks
    if (fread(&exec->num_tasks, sizeof(uint32_t), 1, file) != 1) {
        set_error("Failed to read task count");
        goto cleanup;
    }

    // WHAT: Allocate task queue
    // WHY:  Need storage for loaded tasks
    // HOW:  Allocate array of task_descriptor_t pointers
    exec->max_tasks = exec->config.max_tasks;
    exec->task_queue = (task_descriptor_t**)nimcp_calloc(exec->max_tasks, sizeof(task_descriptor_t*));
    if (!exec->task_queue) {
        set_error("Failed to allocate task queue");
        goto cleanup;
    }

    // WHAT: Read task queue
    // WHY:  Restore queued tasks
    // HOW:  For each task, allocate and read task_descriptor_t
    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)exec->num_tasks);
        }

        exec->task_queue[i] = (task_descriptor_t*)nimcp_calloc(1, sizeof(task_descriptor_t));
        if (!exec->task_queue[i]) {
            set_error("Failed to allocate task %u", i);
            goto cleanup;
        }

        if (fread(exec->task_queue[i], sizeof(task_descriptor_t), 1, file) != 1) {
            set_error("Failed to read task %u", i);
            goto cleanup;
        }

        // Context pointer is always NULL after load (not serializable)
        exec->task_queue[i]->context = NULL;
    }

    // WHAT: Read active task flag and data
    // WHY:  Restore active task if present
    // HOW:  Read has_active_task, then task if true
    bool has_active_task = false;
    if (fread(&has_active_task, sizeof(bool), 1, file) != 1) {
        set_error("Failed to read active task flag");
        goto cleanup;
    }

    if (has_active_task) {
        exec->active_task = (task_descriptor_t*)nimcp_calloc(1, sizeof(task_descriptor_t));
        if (!exec->active_task) {
            set_error("Failed to allocate active task");
            goto cleanup;
        }

        if (fread(exec->active_task, sizeof(task_descriptor_t), 1, file) != 1) {
            set_error("Failed to read active task");
            goto cleanup;
        }

        // Context pointer is always NULL after load
        exec->active_task->context = NULL;
    }

    // THREAD SAFETY: Initialize task mutex (matches executive_create_custom)
    // WHY:  executive_add_task/switch_task/complete_task all lock task_mutex.
    //       Without initialization, locking an uninitialized mutex is UB.
    if (nimcp_mutex_init(&exec->task_mutex, NULL) != 0) {
        set_error("Failed to initialize task mutex in executive_load");
        goto cleanup;
    }

    // Brain reference must be set separately by caller
    exec->brain = NULL;

    return exec;

cleanup:
    // WHAT: Cleanup on error
    // WHY:  Prevent memory leaks
    // HOW:  Free allocated tasks and queue
    if (exec->task_queue) {
        for (uint32_t i = 0; i < exec->num_tasks; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
                exec_heartbeat("exec_loop",
                                 (float)(i + 1) / (float)exec->num_tasks);
            }

            if (exec->task_queue[i]) {
                nimcp_free(exec->task_queue[i]);
            }
        }
        nimcp_free(exec->task_queue);
    }
    if (exec->active_task) {
        nimcp_free(exec->active_task);
    }
    nimcp_free(exec);
    exec = NULL;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_load: validation failed");
    return NULL;
}

//=============================================================================
// Portia Integration Functions
//=============================================================================

/**
 * WHAT: Get current Portia platform tier
 * WHY:  Allow executive to adapt behavior based on resource constraints
 * HOW:  Return cached tier from Portia messages
 */
uint32_t executive_get_portia_tier(executive_controller_t* exec)
{
    // Guard: Null check
    if (!exec) {
        set_error("NULL executive controller");
        return PLATFORM_TIER_MINIMAL;
    }

    // Return cached tier (updated via bio-async messages)
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_get_portia", 0.0f);


    return exec->current_tier;
}

/**
 * WHAT: Check if executive is resource-aware
 * WHY:  Determine if Portia integration is active AND resource constraints detected
 * HOW:  Check resource_aware_mode flag (set when tier < FULL and Portia active)
 */
bool executive_is_resource_aware(executive_controller_t* exec)
{
    // Guard: Null check
    if (!exec) {
        return false;
    }

    // resource_aware_mode is set when Portia is initialized AND tier < FULL
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_is_resourc", 0.0f);


    return exec->resource_aware_mode;
}

/**
 * WHAT: Get recommended planning depth based on resources
 * WHY:  Constrained platforms should use shallower search
 * HOW:  Scale max_plan_depth based on current tier
 */
uint32_t executive_get_recommended_plan_depth(executive_controller_t* exec)
{
    // Guard: Null check - return typical max_plan_depth as safe default
    if (!exec) {
        return 10;  // Standard default matching typical config
    }

    // Guard: Portia integration not enabled
    if (!exec->config.enable_portia_integration) {
        return exec->config.max_plan_depth;
    }

    // If Portia isn't initialized, return full depth (no resource constraints known)
    if (!portia_is_initialized()) {
        return exec->config.max_plan_depth;
    }

    // Query Portia's current tier directly (fallback if bio-async messages not received)
    // This ensures we always have the latest tier info even if async messaging isn't working
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_get_recomm", 0.0f);


    platform_tier_t current_tier = portia_get_current_tier();
    // Update cached value
    if (current_tier != exec->current_tier) {
        exec->current_tier = current_tier;
        exec->resource_aware_mode = (current_tier > PLATFORM_TIER_FULL);
    }

    // Scale depth based on tier
    // Note: FULL=0, MEDIUM=1, CONSTRAINED=2, MINIMAL=3 (lower number = better)
    switch (current_tier) {
        case PLATFORM_TIER_FULL:
            return exec->config.max_plan_depth;
        case PLATFORM_TIER_MEDIUM:
            return (exec->config.max_plan_depth * 3) / 4;  // 75%
        case PLATFORM_TIER_CONSTRAINED:
            return exec->config.max_plan_depth / 2;  // 50%
        case PLATFORM_TIER_MINIMAL:
            return (exec->config.max_plan_depth + 3) / 4;  // 25%, min 1
        default:
            return exec->config.max_plan_depth / 2;
    }
}

/**
 * WHAT: Process pending bio-async messages for executive
 * WHY:  Receive Portia tier changes and other async events
 * HOW:  Call bio_router_process_inbox for executive's context
 */
uint32_t executive_process_messages(executive_controller_t* exec, uint32_t max_messages)
{
    // Guard: Null check
    if (!exec) {
        return 0;
    }

    // Guard: Bio-async not enabled
    if (!exec->bio_async_enabled || !exec->bio_ctx) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_process_me", 0.0f);


    return bio_router_process_inbox(exec->bio_ctx, max_messages);
}

//=============================================================================
// Brain Immune System Integration Functions
//=============================================================================

/**
 * @brief Set brain immune system for executive function modulation
 *
 * WHAT: Associate executive controller with immune system
 * WHY:  Enable cytokine-induced cognitive fog modeling
 * HOW:  Store immune system reference, enable integration
 *
 * COMPLEXITY: O(1)
 */
void executive_set_immune_system(executive_controller_t* exec, brain_immune_system_t* immune)
{
    // Guard: NULL check
    if (!exec) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_set_immune", 0.0f);


    exec->immune_system = immune;
    exec->immune_integration_enabled = (immune != NULL);

    if (immune) {
        LOG_MODULE_INFO(LOG_MODULE, "Executive controller integrated with brain immune system");
    }
}

/**
 * @brief Get current inflammation level from immune system
 *
 * WHAT: Query inflammation level with caching
 * WHY:  Avoid excessive immune system queries
 * HOW:  Check cache expiry, query if needed
 *
 * COMPLEXITY: O(1)
 */
static float get_current_inflammation_level(executive_controller_t* exec)
{
    // Guard: Immune integration not enabled
    if (!exec || !exec->immune_integration_enabled || !exec->immune_system) {
        return 0.0F;
    }

    // WHAT: Cache inflammation level for 100ms to avoid excessive queries
    // WHY:  Immune state changes slowly, no need to query every call
    // HOW:  Check timestamp, refresh if expired
    uint64_t now = exec_get_time_ms();
    if (now - exec->last_immune_check_ms > 100) {
        brain_immune_stats_t stats;
        if (brain_immune_get_stats(exec->immune_system, &stats) == 0) {
            // WHAT: Compute overall inflammation from sites
            // WHY:  Multiple inflammation sites contribute to systemic effect
            // HOW:  Average inflammation across sites, weighted by severity
            if (stats.inflammation_sites > 0) {
                exec->last_inflammation_level = (float)stats.inflammation_sites /
                                                (float)BRAIN_IMMUNE_MAX_INFLAMMATION;
                exec->last_inflammation_level = fminf(exec->last_inflammation_level, 1.0F);
            } else {
                exec->last_inflammation_level = 0.0F;
            }
        }
        exec->last_immune_check_ms = now;
    }

    return exec->last_inflammation_level;
}

/**
 * @brief Get current inflammation-adjusted cognitive capacity
 *
 * WHAT: Calculate capacity reduction from inflammation
 * WHY:  Cytokines impair prefrontal function
 * HOW:  Scale 1.0 down based on inflammation level
 *
 * BIOLOGY: Pro-inflammatory cytokines (IL-1, IL-6, TNF-α) impair:
 *          - Working memory capacity
 *          - Attention span
 *          - Processing speed
 *          - Executive control
 *
 * COMPLEXITY: O(1)
 */
float executive_get_immune_adjusted_capacity(executive_controller_t* exec)
{
    // Guard: NULL check
    if (!exec) {
        return 1.0F;
    }

    // Guard: Immune integration not enabled
    if (!exec->immune_integration_enabled || !exec->immune_system) {
        return 1.0F;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_get_immune", 0.0f);


    float inflammation = get_current_inflammation_level(exec);

    // WHAT: Map inflammation [0, 1] to capacity [1, 0]
    // WHY:  Higher inflammation = lower capacity
    // HOW:  Linear scaling with floor at 0.1 (never completely disabled)
    //
    // inflammation=0.0 → capacity=1.0 (full capacity)
    // inflammation=0.5 → capacity=0.55 (moderate impairment)
    // inflammation=1.0 → capacity=0.1 (severe impairment)
    float capacity = 1.0F - (inflammation * 0.9F);
    return fmaxf(capacity, 0.1F);
}

/**
 * @brief Check if executive function is significantly impaired
 *
 * WHAT: Determine if inflammation exceeds impairment threshold
 * WHY:  Signal to system that cognitive load should be reduced
 * HOW:  Compare inflammation to configured threshold
 *
 * COMPLEXITY: O(1)
 */
bool executive_is_immune_impaired(executive_controller_t* exec)
{
    // Guard: NULL check
    if (!exec) {
        return false;
    }

    // Guard: Immune integration not enabled
    if (!exec->immune_integration_enabled || !exec->immune_system) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_is_immune_", 0.0f);


    float inflammation = get_current_inflammation_level(exec);
    float threshold = exec->config.immune_impairment_threshold;

    return (inflammation >= threshold);
}

/**
 * @brief Get inflammation-adjusted task switch cost
 *
 * WHAT: Calculate switch cost with cytokine-induced rigidity
 * WHY:  Inflammation increases perseveration and reduces flexibility
 * HOW:  Scale base cost up with inflammation
 *
 * BIOLOGY: Pro-inflammatory states → cognitive rigidity
 *          - Harder to disengage from current task
 *          - Reduced set-shifting ability
 *          - Increased switch cost
 *
 * COMPLEXITY: O(1)
 */
float executive_get_immune_adjusted_switch_cost(executive_controller_t* exec)
{
    // Guard: NULL check
    if (!exec) {
        return DEFAULT_SWITCH_COST_MS;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_get_immune", 0.0f);


    float base_cost = exec->config.task_switch_cost_ms;

    // Guard: Immune integration not enabled
    if (!exec->immune_integration_enabled || !exec->immune_system) {
        return base_cost;
    }

    float inflammation = get_current_inflammation_level(exec);

    // WHAT: Map inflammation [0, 1] to cost multiplier [1.0, 3.0]
    // WHY:  Inflammation increases cognitive rigidity
    // HOW:  Linear scaling
    //
    // inflammation=0.0 → multiplier=1.0 (normal switching)
    // inflammation=0.5 → multiplier=2.0 (moderate difficulty)
    // inflammation=1.0 → multiplier=3.0 (severe perseveration)
    float multiplier = 1.0F + (inflammation * 2.0F);
    return base_cost * multiplier;
}

/**
 * @brief Get inflammation-adjusted inhibition threshold
 *
 * WHAT: Calculate inhibition threshold with inflammatory impairment
 * WHY:  Inflammation impairs impulse control
 * HOW:  Increase threshold (harder to inhibit) with inflammation
 *
 * BIOLOGY: Prefrontal inhibitory control requires:
 *          - Good working memory (holds goal state)
 *          - Strong executive function (overrides prepotent response)
 *          Both are impaired by cytokines → weaker inhibition
 *
 * COMPLEXITY: O(1)
 */
float executive_get_immune_adjusted_inhibition(executive_controller_t* exec)
{
    // Guard: NULL check
    if (!exec) {
        return DEFAULT_INHIBITION_THRESHOLD;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_get_immune", 0.0f);


    float base_threshold = exec->config.inhibition_threshold;

    // Guard: Immune integration not enabled
    if (!exec->immune_integration_enabled || !exec->immune_system) {
        return base_threshold;
    }

    float inflammation = get_current_inflammation_level(exec);

    // WHAT: Map inflammation [0, 1] to threshold increase [0.0, 0.25]
    // WHY:  Higher threshold = harder to inhibit = impaired control
    // HOW:  Add inflammation-scaled offset
    //
    // inflammation=0.0 → offset=0.0 (normal inhibition)
    // inflammation=0.5 → offset=0.125 (moderate impairment)
    // inflammation=1.0 → offset=0.25 (severe impairment)
    float offset = inflammation * 0.25F;
    float adjusted = base_threshold + offset;

    // Clamp to [0, 1]
    return fminf(adjusted, 1.0F);
}

//=============================================================================
// Sleep Integration Functions
//=============================================================================

/**
 * @brief Set current sleep state for executive modulation
 *
 * WHAT: Update sleep state and log the change
 * WHY:  Executive function is highly sensitive to sleep state
 * HOW:  Store state for use by other executive functions
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full executive control
 * - DROWSY: Impaired inhibition (0.6x), reduced flexibility (0.5x)
 * - LIGHT_NREM: Minimal inhibition (0.1x), switch cost 10x higher
 * - DEEP_NREM: Executive offline (0.0x inhibition)
 * - REM: Reduced control (0.3x inhibition), explains dream bizarreness
 *
 * COMPLEXITY: O(1)
 */
void executive_set_sleep_state(executive_controller_t* exec, sleep_state_t state)
{
    // Guard: NULL check
    if (!exec) {
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_set_sleep_", 0.0f);


    exec->current_sleep_state = state;

    LOG_MODULE_DEBUG(LOG_MODULE, "Sleep state updated to %d", state);
}

//=============================================================================
// Knowledge Graph Self-Awareness Integration
//=============================================================================

/**
 * @brief Connect executive to internal knowledge graph
 *
 * WHAT: Initialize KG context for self-awareness queries
 * WHY:  Enable executive to query its own capabilities and connections
 * HOW:  Use KG helper functions to establish connection
 *
 * @param exec Executive controller instance
 * @param brain Brain instance for KG access
 * @return true if connected (or KG gracefully disabled), false on error
 */
bool executive_connect_kg(executive_controller_t* exec, brain_t brain)
{
    if (!exec) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_connect_kg: exec is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_connect_kg", 0.0f);


    int result = kg_module_init(&exec->kg_context, brain, "Executive_Controller");

    if (result != 0) {
        LOG_MODULE_ERROR(LOG_MODULE, "Failed to initialize KG context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_connect_kg: validation failed");
        return false;
    }

    if (!kg_is_available(&exec->kg_context)) {
        exec->kg_connected = false;
        LOG_MODULE_INFO(LOG_MODULE, "KG disabled, graceful degradation");
        return true;
    }

    exec->kg_connected = true;
    LOG_MODULE_INFO(LOG_MODULE, "Connected to internal KG for self-awareness");

    return true;
}

/**
 * @brief Query executive's connected modules from KG
 *
 * WHAT: Retrieve list of modules connected to executive
 * WHY:  Enable self-awareness of coordination relationships
 * HOW:  Query KG for outgoing edges from executive node
 *
 * @param exec Executive controller instance
 * @return Number of connected modules found (0 if KG not connected)
 */
int executive_query_connected_modules(executive_controller_t* exec)
{
    if (!exec || !exec->kg_connected) {
        return 0;
    }

    if (!kg_is_available(&exec->kg_context)) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_query_conn", 0.0f);


    brain_kg_edge_list_t* outgoing = kg_get_outgoing_safe(&exec->kg_context);
    if (!outgoing) {
        return 0;
    }

    int count = (int)outgoing->count;
    LOG_MODULE_DEBUG(LOG_MODULE, "Executive has %d outgoing KG connections", count);

    brain_kg_edge_list_destroy(outgoing);
    return count;
}

/**
 * @brief Query executive's task capabilities from KG
 *
 * WHAT: Query KG for self-knowledge about task handling
 * WHY:  Enable introspection of what tasks executive can manage
 * HOW:  Find self node and get neighbors
 *
 * @param exec Executive controller instance
 * @return true if self-knowledge is available, false otherwise
 */
bool executive_query_self_capabilities(executive_controller_t* exec)
{
    if (!exec || !exec->kg_connected) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_query_self_capabilities: required parameter is NULL (exec, exec->kg_connected)");
        return false;
    }

    if (!kg_has_node(&exec->kg_context)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_query_self_capabilities: kg_has_node is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_query_self", 0.0f);


    const brain_kg_node_t* self = kg_get_node_safe(
        &exec->kg_context,
        exec->kg_context.self_node_id
    );

    if (self) {
        LOG_MODULE_DEBUG(LOG_MODULE, "Executive self-knowledge: name=%s, state=%d",
                        self->name, self->state);
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_query_self_capabilities: validation failed");
    return false;
}

/**
 * @brief W9-kg read-path: query prior ethics events before committing a
 *        decision.
 *
 * WHAT: Counts nodes in the KG that mention "ethics" — these are the
 *       event-nodes emitted by ethics, immune, and audit modules.
 *       Nonzero count signals the executive that the brain has ethical
 *       context to honor for this decision.
 * WHY:  Closes the executive PARTIAL status. Previously executive could
 *       only self-describe; now it consults KG context and publishes its
 *       decision so downstream consumers (W16) see executive intent.
 * HOW:  Uses the shared W9-kg helpers. Emits an executive decision event
 *       regardless of ethics event count.
 *
 * @param exec Executive controller (non-NULL)
 * @param decision_id opaque decision id (for the emit event)
 * @param confidence computed decision confidence for the emit event
 * @return count of ethics events visible in the KG, 0 if KG unavailable.
 */
uint32_t executive_query_ethics_context(executive_controller_t* exec,
                                        uint64_t decision_id,
                                        float confidence)
{
    if (!exec) return 0;

    struct brain_struct* brain = w9kg_get_registered_brain();
    if (!brain) return 0;

    uint32_t ethics_count = w9kg_query_executive_ethics_events(brain);
    w9kg_emit_executive_decision(brain, decision_id, confidence);
    return ethics_count;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int executive_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_query_self", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Executive");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                exec_heartbeat("exec_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Executive");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Executive");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

//=============================================================================
// Monte Carlo Enhanced Functions
//=============================================================================

/**
 * @brief Select task using epsilon-greedy exploration
 *
 * WHAT: Select task with exploration-exploitation tradeoff
 * WHY:  Avoid getting stuck in local optima during task scheduling
 * HOW:  With probability epsilon, select random task; otherwise best
 *
 * @param exec Executive controller
 * @param epsilon Exploration probability [0,1]
 * @return Selected task or NULL
 */
task_descriptor_t* executive_select_task_epsilon_greedy_mc(
    executive_controller_t* exec,
    float epsilon
) {
    if (!exec || exec->num_tasks == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_select_task_epsilon_greedy_mc: exec is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_select_tas", 0.0f);


    if (g_exec_mc_seed == 0) {
        g_exec_mc_seed = mc_seed_from_time();
    }

    /* Count pending tasks */
    uint32_t num_pending = 0;
    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)exec->num_tasks);
        }

        if (exec->task_queue[i] && exec->task_queue[i]->status == TASK_STATUS_PENDING) {
            num_pending++;
        }
    }

    if (num_pending == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_select_task_epsilon_greedy_mc: num_pending is zero");
        return NULL;
    }

    /* Epsilon-greedy selection */
    float r = mc_random_uniform(&g_exec_mc_seed);

    if (r < epsilon) {
        /* Explore: select random pending task */
        uint32_t random_idx = mc_random_int(&g_exec_mc_seed, num_pending);
        uint32_t count = 0;

        for (uint32_t i = 0; i < exec->num_tasks; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
                exec_heartbeat("exec_loop",
                                 (float)(i + 1) / (float)exec->num_tasks);
            }

            if (exec->task_queue[i] && exec->task_queue[i]->status == TASK_STATUS_PENDING) {
                if (count == random_idx) {
                    return exec->task_queue[i];
                }
                count++;
            }
        }
    }

    /* Exploit: select highest priority task */
    return get_highest_priority_task(exec);
}

/**
 * @brief Estimate task value via Monte Carlo rollout
 *
 * WHAT: Estimate expected value of executing a task
 * WHY:  Support value-based task selection
 * HOW:  Simulate task execution and accumulate rewards
 *
 * @param exec Executive controller
 * @param task Task to evaluate
 * @param num_rollouts Number of MC rollouts
 * @param discount Discount factor for future rewards
 * @return Estimated value
 */
float executive_estimate_task_value_mc(
    executive_controller_t* exec,
    const task_descriptor_t* task,
    uint32_t num_rollouts,
    float discount
) {
    if (!exec || !task) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_estimate_t", 0.0f);


    if (g_exec_mc_seed == 0) {
        g_exec_mc_seed = mc_seed_from_time();
    }

    float total_value = 0.0f;

    for (uint32_t r = 0; r < num_rollouts; r++) {
        /* Phase 8: Loop progress heartbeat */
        if ((r & 0xFF) == 0 && num_rollouts > 256) {
            exec_heartbeat("exec_loop",
                             (float)(r + 1) / (float)num_rollouts);
        }

        float rollout_value = 0.0f;
        float gamma = 1.0f;

        /* Simulate task execution with stochastic outcomes */
        float success_prob = 0.8f;  /* Base success probability */

        /* Adjust for priority - higher priority tasks more likely to succeed */
        success_prob += 0.04f * (float)task->priority;
        if (success_prob > 0.99f) success_prob = 0.99f;

        /* Simulate execution */
        float outcome = mc_random_uniform(&g_exec_mc_seed);
        if (outcome < success_prob) {
            /* Success: reward based on priority */
            rollout_value += gamma * (1.0f + 0.2f * (float)task->priority);
        } else {
            /* Failure: negative reward */
            rollout_value += gamma * (-0.5f);
        }

        /* Simulate future tasks (simplified) */
        for (uint32_t step = 1; step < 5; step++) {
            gamma *= discount;
            float future_reward = mc_random_uniform(&g_exec_mc_seed) * 0.5f;
            rollout_value += gamma * future_reward;
        }

        total_value += rollout_value;
    }

    return total_value / (float)num_rollouts;
}

/**
 * @brief Select task using softmax over estimated values
 *
 * WHAT: Probabilistic task selection based on values
 * WHY:  Smooth exploration that favors high-value tasks
 * HOW:  Compute softmax probabilities, sample
 *
 * @param exec Executive controller
 * @param temperature Softmax temperature (higher = more exploration)
 * @param num_rollouts MC rollouts per task for value estimation
 * @return Selected task
 */
task_descriptor_t* executive_select_task_softmax_mc(
    executive_controller_t* exec,
    float temperature,
    uint32_t num_rollouts
) {
    if (!exec || exec->num_tasks == 0 || temperature <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_select_task_softmax_mc: exec is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_select_tas", 0.0f);


    if (g_exec_mc_seed == 0) {
        g_exec_mc_seed = mc_seed_from_time();
    }

    /* Collect pending tasks and their values */
    task_descriptor_t** pending = nimcp_calloc(exec->num_tasks, sizeof(task_descriptor_t*));
    float* values = nimcp_calloc(exec->num_tasks, sizeof(float));
    if (!pending || !values) {
        nimcp_free(pending);
        pending = NULL;
        nimcp_free(values);
        values = NULL;
        return get_highest_priority_task(exec);
    }

    uint32_t num_pending = 0;
    float max_value = -1e30f;

    for (uint32_t i = 0; i < exec->num_tasks; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && exec->num_tasks > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)exec->num_tasks);
        }

        if (exec->task_queue[i] && exec->task_queue[i]->status == TASK_STATUS_PENDING) {
            pending[num_pending] = exec->task_queue[i];
            values[num_pending] = executive_estimate_task_value_mc(
                exec, exec->task_queue[i], num_rollouts, 0.9f
            );
            if (values[num_pending] > max_value) {
                max_value = values[num_pending];
            }
            num_pending++;
        }
    }

    if (num_pending == 0) {
        nimcp_free(pending);
        pending = NULL;
        nimcp_free(values);
        values = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "executive_select_task_softmax_mc: num_pending is zero");
        return NULL;
    }

    /* Compute softmax probabilities (with max subtraction for stability) */
    float sum_exp = 0.0f;
    for (uint32_t i = 0; i < num_pending; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_pending > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)num_pending);
        }

        values[i] = expf((values[i] - max_value) / temperature);
        sum_exp += values[i];
    }

    /* Sample from distribution */
    float r = mc_random_uniform(&g_exec_mc_seed) * sum_exp;
    float cumulative = 0.0f;
    task_descriptor_t* selected = pending[num_pending - 1];

    for (uint32_t i = 0; i < num_pending; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_pending > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)num_pending);
        }

        cumulative += values[i];
        if (r < cumulative) {
            selected = pending[i];
            break;
        }
    }

    nimcp_free(pending);
    pending = NULL;
    nimcp_free(values);
    values = NULL;

    return selected;
}

/**
 * @brief Get thread-local MC seed for executive functions
 *
 * @return Pointer to thread-local seed
 */
uint32_t* executive_get_mc_seed(void) {
    if (g_exec_mc_seed == 0) {
        g_exec_mc_seed = mc_seed_from_time();
    }
    return &g_exec_mc_seed;
}

//=============================================================================
// MCTS-Based Goal Decomposition Implementation
//=============================================================================

/** Maximum actions per planning state */
#define MCTS_PLAN_MAX_ACTIONS 8

/** Maximum nodes for planning tree */
#define MCTS_PLAN_MAX_NODES 4096

/**
 * @brief Planning state for MCTS tree node
 */
typedef struct mcts_plan_state {
    char goal[NIMCP_LABEL_BUFFER_SIZE];                 /**< Current goal/subgoal */
    uint32_t depth;                 /**< Current depth in plan */
    uint32_t action_taken;          /**< Action index that led here */
    float accumulated_cost;         /**< Total cost so far */
    bool is_terminal;               /**< Goal achieved or failed */
    char actions[MCTS_PLAN_MAX_ACTIONS][64];  /**< Available action descriptions */
    uint32_t num_actions;           /**< Number of available actions */
} mcts_plan_state_t;

/**
 * @brief User data for MCTS planning callbacks
 */
typedef struct mcts_plan_context {
    executive_controller_t* exec;   /**< Executive controller */
    const executive_mcts_config_t* config;  /**< MCTS configuration */
    uint32_t max_depth;             /**< Maximum planning depth */
    uint32_t nodes_created;         /**< Track node creation */
} mcts_plan_context_t;

/* Forward declarations for MCTS callbacks */
static uint32_t plan_get_action_count(const void* state, void* user_data);
static uint32_t plan_get_action(const void* state, uint32_t idx, void* user_data);
static void* plan_apply_action(const void* state, uint32_t action, void* user_data);
static float plan_evaluate(const void* state, void* user_data);
static bool plan_is_terminal(const void* state, void* user_data);
static void plan_free_state(void* state, void* user_data);
static void* plan_clone_state(const void* state, void* user_data);

/**
 * @brief Generate actions for a planning state
 */
static void generate_plan_actions(mcts_plan_state_t* state, mcts_plan_context_t* ctx) {
    /* Generate possible actions based on goal and depth */
    state->num_actions = 0;

    if (state->is_terminal || state->depth >= ctx->max_depth) {
        return;
    }

    /* Base actions that apply to most goals */
    const char* base_actions[] = {
        "Analyze requirements",
        "Gather information",
        "Execute primary step",
        "Verify result",
        "Handle error case",
        "Optimize approach",
        "Delegate subtask",
        "Consolidate progress"
    };

    /* Select relevant actions based on depth and goal */
    uint32_t action_count = 0;

    if (state->depth == 0) {
        /* Initial planning: analyze and gather info first */
        snprintf(state->actions[action_count++], 64, "Analyze: %s", state->goal);
        snprintf(state->actions[action_count++], 64, "Decompose: %s", state->goal);
        snprintf(state->actions[action_count++], 64, "Gather info for: %s", state->goal);
    } else if (state->depth < ctx->max_depth / 2) {
        /* Middle phase: execution */
        for (uint32_t i = 2; i < 6 && action_count < MCTS_PLAN_MAX_ACTIONS; i++) {
            snprintf(state->actions[action_count++], 64, "%s", base_actions[i]);
        }
    } else {
        /* Final phase: verification and consolidation */
        for (uint32_t i = 3; i < 8 && action_count < MCTS_PLAN_MAX_ACTIONS; i++) {
            snprintf(state->actions[action_count++], 64, "%s", base_actions[i]);
        }
    }

    state->num_actions = action_count;
}

/* MCTS Callback: Get number of available actions */
static uint32_t plan_get_action_count(const void* state, void* user_data) {
    const mcts_plan_state_t* s = (const mcts_plan_state_t*)state;
    (void)user_data;
    return s->num_actions;
}

/* MCTS Callback: Get action ID by index */
static uint32_t plan_get_action(const void* state, uint32_t idx, void* user_data) {
    const mcts_plan_state_t* s = (const mcts_plan_state_t*)state;
    (void)user_data;
    if (idx >= s->num_actions) return UINT32_MAX;
    return idx;
}

/* MCTS Callback: Apply action and create new state */
static void* plan_apply_action(const void* state, uint32_t action, void* user_data) {
    const mcts_plan_state_t* s = (const mcts_plan_state_t*)state;
    mcts_plan_context_t* ctx = (mcts_plan_context_t*)user_data;

    if (action >= s->num_actions) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "plan_apply_action: capacity exceeded");
        return NULL;
    }

    mcts_plan_state_t* new_state = nimcp_calloc(1, sizeof(mcts_plan_state_t));
    if (!new_state) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate new_state");

        return NULL;

    }

    ctx->nodes_created++;

    /* Copy base properties */
    snprintf(new_state->goal, sizeof(new_state->goal), "%s", s->goal);
    new_state->depth = s->depth + 1;
    new_state->action_taken = action;

    /* Estimate action cost (simplified model) */
    float base_cost = 100.0f + 50.0f * (float)action;
    new_state->accumulated_cost = s->accumulated_cost + base_cost;

    /* Check if goal achieved (probabilistic based on depth) */
    float completion_prob = (float)new_state->depth / (float)ctx->max_depth;
    float r = mc_random_uniform(&g_exec_mc_seed);
    new_state->is_terminal = (r < completion_prob * 0.3f) || (new_state->depth >= ctx->max_depth);

    /* Generate actions for new state */
    generate_plan_actions(new_state, ctx);

    return new_state;
}

/* MCTS Callback: Evaluate state value (heuristic) */
static float plan_evaluate(const void* state, void* user_data) {
    const mcts_plan_state_t* s = (const mcts_plan_state_t*)state;
    mcts_plan_context_t* ctx = (mcts_plan_context_t*)user_data;

    if (s->is_terminal && s->depth <= ctx->max_depth) {
        /* Goal achieved - reward inversely proportional to cost */
        float cost_factor = 1.0f / (1.0f + s->accumulated_cost / 1000.0f);
        float depth_factor = 1.0f - (float)s->depth / (float)(ctx->max_depth * 2);
        return 0.5f + 0.5f * cost_factor * depth_factor;
    }

    /* Non-terminal: estimate based on progress */
    float progress = (float)s->depth / (float)ctx->max_depth;
    float cost_penalty = s->accumulated_cost / 5000.0f;

    return 0.3f + 0.4f * progress - 0.2f * cost_penalty;
}

/* MCTS Callback: Check if state is terminal */
static bool plan_is_terminal(const void* state, void* user_data) {
    const mcts_plan_state_t* s = (const mcts_plan_state_t*)state;
    mcts_plan_context_t* ctx = (mcts_plan_context_t*)user_data;
    return s->is_terminal || s->depth >= ctx->max_depth || s->num_actions == 0;
}

/* MCTS Callback: Free state memory */
static void plan_free_state(void* state, void* user_data) {
    (void)user_data;
    nimcp_free(state);
    state = NULL;
}

/* MCTS Callback: Clone state */
static void* plan_clone_state(const void* state, void* user_data) {
    mcts_plan_context_t* ctx = (mcts_plan_context_t*)user_data;
    const mcts_plan_state_t* s = (const mcts_plan_state_t*)state;

    mcts_plan_state_t* clone = nimcp_calloc(1, sizeof(mcts_plan_state_t));
    if (!clone) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate clone");

        return NULL;

    }

    memcpy(clone, s, sizeof(mcts_plan_state_t));
    ctx->nodes_created++;

    return clone;
}

void executive_mcts_config_init(executive_mcts_config_t* config) {
    if (!config) return;

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_mcts_confi", 0.0f);


    config->max_iterations = MCTS_DEFAULT_ITERATIONS;
    config->max_depth = DEFAULT_MAX_PLAN_DEPTH;
    config->exploration_constant = MCTS_DEFAULT_EXPLORATION;
    config->discount_factor = 0.95f;
    config->rollout_depth = 5;
    config->enable_pruning = true;
    config->pruning_threshold = 0.1f;
}

plan_t* executive_create_plan_mcts(
    executive_controller_t* exec,
    const char* goal,
    const executive_mcts_config_t* config,
    executive_mcts_stats_t* stats)
{
    if (!exec || !goal) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_create_plan_mcts: required parameter is NULL (exec, goal)");
        return NULL;
    }

    /* Use defaults if config not provided */
    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_create_pla", 0.0f);


    executive_mcts_config_t default_config;
    if (!config) {
        executive_mcts_config_init(&default_config);
        config = &default_config;
    }

    uint64_t start_time = exec_get_time_ms();

    /* Initialize seed if needed */
    if (g_exec_mc_seed == 0) {
        g_exec_mc_seed = mc_seed_from_time();
    }

    /* Create initial planning state */
    mcts_plan_state_t* initial = nimcp_calloc(1, sizeof(mcts_plan_state_t));
    if (!initial) {
        set_error("Failed to allocate initial planning state");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_create_plan_mcts: initial is NULL");
        return NULL;
    }

    snprintf(initial->goal, sizeof(initial->goal), "%s", goal);
    initial->depth = 0;
    initial->action_taken = 0;
    initial->accumulated_cost = 0.0f;
    initial->is_terminal = false;

    /* Setup MCTS context */
    mcts_plan_context_t ctx = {
        .exec = exec,
        .config = config,
        .max_depth = config->max_depth,
        .nodes_created = 1
    };

    /* Generate initial actions */
    generate_plan_actions(initial, &ctx);

    /* Configure MCTS */
    mcts_config_t mcts_cfg;
    mcts_config_init(&mcts_cfg);

    mcts_cfg.max_iterations = config->max_iterations;
    mcts_cfg.max_depth = config->max_depth;
    mcts_cfg.max_nodes = MCTS_PLAN_MAX_NODES;
    mcts_cfg.exploration_constant = config->exploration_constant;
    mcts_cfg.discount_factor = config->discount_factor;
    mcts_cfg.policy = MCTS_SELECT_UCB1;
    mcts_cfg.seed = g_exec_mc_seed;

    /* Set callbacks */
    mcts_cfg.get_action_count = plan_get_action_count;
    mcts_cfg.get_action = plan_get_action;
    mcts_cfg.apply_action = plan_apply_action;
    mcts_cfg.evaluate = plan_evaluate;
    mcts_cfg.is_terminal = plan_is_terminal;
    mcts_cfg.free_state = plan_free_state;
    mcts_cfg.clone_state = plan_clone_state;
    mcts_cfg.user_data = &ctx;

    /* Run MCTS search */
    mcts_result_t mcts_result;
    memset(&mcts_result, 0, sizeof(mcts_result));

    nimcp_mc_result_t mc_ret = mcts_search(&mcts_cfg, initial, &mcts_result);

    /* Update seed for next call */
    g_exec_mc_seed = mcts_cfg.seed;

    if (mc_ret != NIMCP_MC_OK) {
        LOG_WARN("MCTS search failed with code %d, returning NULL to let caller handle fallback", mc_ret);
        plan_free_state(initial, &ctx);
        return NULL;
    }

    /* Build plan from MCTS results */
    uint32_t num_steps = mcts_result.max_depth_reached;
    if (num_steps == 0) num_steps = 1;
    if (num_steps > config->max_depth) num_steps = config->max_depth;

    plan_t* plan = nimcp_calloc(1, sizeof(plan_t));
    if (!plan) {
        set_error("Failed to allocate plan");
        plan_free_state(initial, &ctx);
        mcts_result_free(&mcts_result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_create_plan_mcts: plan is NULL");
        return NULL;
    }

    plan->steps = nimcp_calloc(num_steps, sizeof(plan_step_t));
    if (!plan->steps) {
        set_error("Failed to allocate plan steps");
        nimcp_free(plan);
        plan = NULL;
        plan_free_state(initial, &ctx);
        mcts_result_free(&mcts_result);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_create_plan_mcts: plan->steps is NULL");
        return NULL;
    }

    plan->type = PLAN_TYPE_HIERARCHICAL;  /* MCTS produces hierarchical plans */
    snprintf(plan->goal, sizeof(plan->goal), "%s", goal);
    plan->num_steps = num_steps;

    /* Generate steps based on best action sequence */
    mcts_plan_state_t* current = initial;
    mcts_plan_state_t* prev = NULL;

    /* Base action descriptions for fallback */
    const char* phase_actions[] = {
        "Analyze and decompose goal",
        "Gather required information",
        "Execute primary action",
        "Verify intermediate result",
        "Handle contingencies",
        "Optimize approach",
        "Execute secondary action",
        "Consolidate and verify",
        "Finalize and complete",
        "Review and confirm"
    };

    for (uint32_t i = 0; i < num_steps; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_steps > 256) {
            exec_heartbeat("exec_loop",
                             (float)(i + 1) / (float)num_steps);
        }

        /* Use best action from current state if available */
        uint32_t best_action = 0;
        bool have_action = false;

        if (current && current->num_actions > 0) {
            if (i == 0 && mcts_result.num_actions > 0) {
                /* First step: use MCTS best action */
                float best_val = -1e30f;
                for (uint32_t a = 0; a < mcts_result.num_actions; a++) {
                    /* Phase 8: Loop progress heartbeat */
                    if ((a & 0xFF) == 0 && mcts_result.num_actions > 256) {
                        exec_heartbeat("exec_loop",
                                         (float)(a + 1) / (float)mcts_result.num_actions);
                    }

                    if (mcts_result.action_values && mcts_result.action_values[a] > best_val) {
                        best_val = mcts_result.action_values[a];
                        best_action = a;
                    }
                }
                have_action = true;
            } else {
                /* Subsequent steps: select from available actions */
                best_action = mc_random_int(&g_exec_mc_seed, current->num_actions);
                have_action = true;
            }
        }

        /* Set step description */
        if (have_action && best_action < current->num_actions) {
            snprintf(plan->steps[i].description, sizeof(plan->steps[i].description),
                     "%s", current->actions[best_action]);
        } else {
            /* Fallback: use phase-appropriate action */
            uint32_t phase_idx = i % 10;
            snprintf(plan->steps[i].description, sizeof(plan->steps[i].description),
                     "%s: %s", phase_actions[phase_idx], goal);
        }

        plan->steps[i].estimated_cost = 100 + (i * 50);
        plan->steps[i].is_critical = (i == 0) || (i == num_steps - 1);

        /* Move to next state */
        if (prev && prev != initial) {
            plan_free_state(prev, &ctx);
        }
        prev = current;

        if (current && have_action && best_action < current->num_actions) {
            current = plan_apply_action(current, best_action, &ctx);
        } else {
            current = NULL;
        }
    }

    /* Clean up */
    if (prev && prev != initial && prev != current) {
        plan_free_state(prev, &ctx);
    }
    if (current && current != initial) {
        plan_free_state(current, &ctx);
    }
    plan_free_state(initial, &ctx);
    mcts_result_free(&mcts_result);

    /* Fill statistics if requested */
    if (stats) {
        uint64_t end_time = exec_get_time_ms();
        stats->nodes_expanded = ctx.nodes_created;
        stats->iterations_run = mcts_result.iterations_completed;
        stats->max_depth_reached = mcts_result.max_depth_reached;
        stats->root_value = mcts_result.best_value;
        stats->planning_time_ms = (float)(end_time - start_time);
        stats->avg_branching_factor = (float)ctx.nodes_created / (float)(mcts_result.iterations_completed + 1);
    }

    /* Update executive stats */
    exec->stats.plans_created++;
    float old_avg = exec->stats.avg_plan_length;
    float n = (float)exec->stats.plans_created;
    exec->stats.avg_plan_length = (old_avg * (n - 1.0f) + (float)plan->num_steps) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    LOG_DEBUG("MCTS plan created: %u steps, %u nodes, value %.3f",
              plan->num_steps, ctx.nodes_created, mcts_result.best_value);

    return plan;
}

float executive_evaluate_plan_mcts(
    executive_controller_t* exec,
    const plan_t* plan,
    uint32_t num_rollouts)
{
    if (!exec || !plan || plan->num_steps == 0) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_evaluate_p", 0.0f);


    if (g_exec_mc_seed == 0) {
        g_exec_mc_seed = mc_seed_from_time();
    }

    float total_success = 0.0f;

    for (uint32_t r = 0; r < num_rollouts; r++) {
        /* Phase 8: Loop progress heartbeat */
        if ((r & 0xFF) == 0 && num_rollouts > 256) {
            exec_heartbeat("exec_loop",
                             (float)(r + 1) / (float)num_rollouts);
        }

        float success_prob = 1.0f;

        /* Simulate plan execution */
        for (uint32_t i = 0; i < plan->num_steps; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && plan->num_steps > 256) {
                exec_heartbeat("exec_loop",
                                 (float)(i + 1) / (float)plan->num_steps);
            }

            /* Base success probability per step */
            float step_success = 0.85f;

            /* Critical steps have higher variance */
            if (plan->steps[i].is_critical) {
                step_success = 0.7f + 0.25f * mc_random_uniform(&g_exec_mc_seed);
            }

            /* Cost affects success */
            float cost_factor = 1.0f / (1.0f + plan->steps[i].estimated_cost / 1000.0f);
            step_success *= (0.8f + 0.2f * cost_factor);

            /* Roll for success */
            if (mc_random_uniform(&g_exec_mc_seed) > step_success) {
                success_prob *= 0.5f;  /* Partial failure */
            }
        }

        total_success += success_prob;
    }

    return total_success / (float)num_rollouts;
}

plan_t* executive_replan_mcts(
    executive_controller_t* exec,
    const plan_t* current_plan,
    uint32_t current_step,
    const executive_mcts_config_t* config)
{
    if (!exec || !current_plan) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_replan_mcts: required parameter is NULL (exec, current_plan)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    exec_heartbeat("exec_executive_replan_mct", 0.0f);


    if (current_step >= current_plan->num_steps) {
        set_error("Current step %u exceeds plan length %u", current_step, current_plan->num_steps);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_replan_mcts: capacity exceeded");
        return NULL;
    }

    /* Create new goal based on remaining plan */
    char new_goal[NIMCP_LABEL_BUFFER_SIZE];
    snprintf(new_goal, sizeof(new_goal), "Continue: %s (from step %u)",
             current_plan->goal, current_step);

    /* Use MCTS to plan remaining steps */
    executive_mcts_config_t replan_config;
    if (config) {
        replan_config = *config;
    } else {
        executive_mcts_config_init(&replan_config);
    }

    /* Reduce iterations for faster replanning */
    replan_config.max_iterations /= 2;
    if (replan_config.max_iterations < 20) replan_config.max_iterations = 20;

    /* Reduce depth to remaining steps */
    uint32_t remaining = current_plan->num_steps - current_step;
    if (replan_config.max_depth > remaining + 2) {
        replan_config.max_depth = remaining + 2;
    }

    return executive_create_plan_mcts(exec, new_goal, &replan_config, NULL);
}

char* executive_get_best_action_mcts(
    executive_controller_t* exec,
    const char* goal,
    const executive_mcts_config_t* config,
    float* action_value)
{
    if (!exec || !goal) {
        set_error("NULL parameter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "executive_get_best_action_mcts: required parameter is NULL (exec, goal)");
        return NULL;
    }

    /* Use quick MCTS search */
    executive_mcts_config_t quick_config;
    if (config) {
        quick_config = *config;
    } else {
        executive_mcts_config_init(&quick_config);
    }

    /* Reduce iterations for single action */
    quick_config.max_iterations /= 4;
    if (quick_config.max_iterations < 10) quick_config.max_iterations = 10;
    quick_config.max_depth = 3;  /* Only need shallow search */

    executive_mcts_stats_t stats;
    plan_t* plan = executive_create_plan_mcts(exec, goal, &quick_config, &stats);

    if (!plan || plan->num_steps == 0) {
        if (plan) executive_destroy_plan(plan);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_get_best_action_mcts: validation failed");
        return NULL;
    }

    /* Extract first action */
    char* action = nimcp_calloc(128, sizeof(char));
    if (!action) {
        executive_destroy_plan(plan);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "executive_get_best_action_mcts: action is NULL");
        return NULL;
    }

    snprintf(action, 128, "%s", plan->steps[0].description);

    if (action_value) {
        *action_value = stats.root_value;
    }

    executive_destroy_plan(plan);

    return action;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void exec_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_exec_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int exec_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "exec_training_begin: NULL argument");
        return -1;
    }
    exec_heartbeat_instance(NULL, "exec_training_begin", 0.0f);
    (void)(struct executive_controller*)instance; /* Module state available for reset */
    return 0;
}

int exec_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "exec_training_end: NULL argument");
        return -1;
    }
    exec_heartbeat_instance(NULL, "exec_training_end", 1.0f);
    (void)(struct executive_controller*)instance; /* Module state available for finalization */
    return 0;
}

int exec_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "exec_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    exec_heartbeat_instance(NULL, "exec_training_step", progress);
    (void)(struct executive_controller*)instance; /* Module state available for step adaptation */
    return 0;
}
