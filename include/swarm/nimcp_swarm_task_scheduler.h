//=============================================================================
// nimcp_swarm_task_scheduler.h - Capability-Aware Task Scheduler
//=============================================================================
/**
 * @file nimcp_swarm_task_scheduler.h
 * @brief Intelligent task-to-agent matching and scheduling
 *
 * WHAT: Capability-aware task scheduling across swarm agents
 * WHY:  Optimal work distribution based on agent capabilities
 * HOW:  Multiple scheduling algorithms with capability matching
 *
 * BIOLOGICAL INSPIRATION:
 * Modeled after ant colony task allocation:
 * - Response threshold model (agents respond based on stimulus level)
 * - Self-organized task selection (no central controller needed)
 * - Capability specialization (some agents better at certain tasks)
 *
 * SCHEDULING ALGORITHMS:
 * 1. ROUND_ROBIN: Simple fair distribution
 * 2. CAPABILITY_MATCH: Best fit based on required capabilities
 * 3. LOAD_BALANCE: Minimize maximum agent load
 * 4. ENERGY_AWARE: Prefer high-energy agents
 * 5. LOCALITY_AWARE: Prefer nearby agents
 * 6. DEADLINE_DRIVEN: EDF (Earliest Deadline First)
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_SWARM_TASK_SCHEDULER_H
#define NIMCP_SWARM_TASK_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "swarm/nimcp_swarm_task.h"
#include "swarm/nimcp_swarm_task_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum agents tracked by scheduler */
#define SWARM_SCHEDULER_MAX_AGENTS 128

/** Maximum pending tasks in scheduler queue */
#define SWARM_SCHEDULER_MAX_PENDING 512

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Scheduling algorithm selection
 */
typedef enum {
    /**
     * ROUND_ROBIN: Distribute tasks evenly in rotation
     * Pros: Fair, simple
     * Cons: Ignores capabilities, load
     */
    SWARM_SCHEDULER_ROUND_ROBIN = 0,

    /**
     * CAPABILITY_MATCH: Best capability fit for task
     * Pros: Tasks go to most capable agents
     * Cons: May overload specialists
     */
    SWARM_SCHEDULER_CAPABILITY_MATCH,

    /**
     * LOAD_BALANCE: Minimize max agent load
     * Pros: Even workload distribution
     * Cons: May assign to less capable agents
     */
    SWARM_SCHEDULER_LOAD_BALANCE,

    /**
     * ENERGY_AWARE: Prefer high-energy agents
     * Pros: Maximizes uptime, avoids low-battery agents
     * Cons: May underutilize low-energy agents
     */
    SWARM_SCHEDULER_ENERGY_AWARE,

    /**
     * LOCALITY_AWARE: Prefer nearby agents
     * Pros: Minimizes travel time
     * Cons: May overload local agents
     */
    SWARM_SCHEDULER_LOCALITY_AWARE,

    /**
     * DEADLINE_DRIVEN: Earliest Deadline First
     * Pros: Maximizes deadline compliance
     * Cons: May starve low-priority tasks
     */
    SWARM_SCHEDULER_DEADLINE_DRIVEN,

    /**
     * HYBRID: Weighted combination of factors
     * Pros: Balanced decisions
     * Cons: More complex tuning
     */
    SWARM_SCHEDULER_HYBRID,

    SWARM_SCHEDULER_ALGORITHM_COUNT
} swarm_scheduler_algorithm_t;

/**
 * @brief Scheduling decision result
 */
typedef enum {
    SWARM_SCHEDULE_SUCCESS = 0,        /**< Task scheduled successfully */
    SWARM_SCHEDULE_NO_CAPABLE_AGENT,   /**< No agent has required capabilities */
    SWARM_SCHEDULE_ALL_AGENTS_BUSY,    /**< All capable agents at capacity */
    SWARM_SCHEDULE_DEPENDENCIES_UNMET, /**< Task dependencies not satisfied */
    SWARM_SCHEDULE_DEADLINE_INFEASIBLE,/**< Cannot meet deadline */
    SWARM_SCHEDULE_INVALID_TASK,       /**< Invalid task state */
    SWARM_SCHEDULE_ERROR               /**< Internal error */
} swarm_schedule_result_t;

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Hybrid scheduler weights
 *
 * When using SWARM_SCHEDULER_HYBRID, these weights determine
 * how each factor contributes to the final scoring.
 */
typedef struct {
    float capability_weight;           /**< Weight for capability match [0-1] */
    float load_weight;                 /**< Weight for load balancing [0-1] */
    float energy_weight;               /**< Weight for energy level [0-1] */
    float locality_weight;             /**< Weight for proximity [0-1] */
    float deadline_weight;             /**< Weight for deadline urgency [0-1] */
} swarm_scheduler_weights_t;

/**
 * @brief Scheduler configuration
 */
typedef struct {
    /** Scheduling algorithm */
    swarm_scheduler_algorithm_t algorithm;

    /** Weights for hybrid algorithm */
    swarm_scheduler_weights_t weights;

    /** Maximum tasks per agent */
    uint32_t max_tasks_per_agent;

    /** Minimum energy to accept tasks */
    float min_energy_threshold;

    /** Enable automatic scheduling on task submit */
    bool auto_schedule;

    /** Enable bio-async notifications */
    bool enable_bio_async;

    /** Re-evaluate pending tasks periodically */
    bool enable_rescheduling;

    /** Reschedule interval (ms) */
    uint32_t reschedule_interval_ms;
} swarm_scheduler_config_t;

/**
 * @brief Scheduler statistics
 */
typedef struct {
    uint64_t total_scheduled;          /**< Tasks successfully scheduled */
    uint64_t total_failed;             /**< Failed scheduling attempts */
    uint64_t capability_failures;      /**< No capable agent available */
    uint64_t load_failures;            /**< All agents at capacity */
    uint64_t deadline_failures;        /**< Deadline infeasible */

    float avg_scheduling_time_us;      /**< Average scheduling decision time */
    float avg_queue_depth;             /**< Average pending queue depth */

    /** Per-algorithm statistics */
    uint64_t algorithm_uses[SWARM_SCHEDULER_ALGORITHM_COUNT];
} swarm_scheduler_stats_t;

/**
 * @brief Agent score for scheduling decisions
 */
typedef struct {
    uint32_t agent_id;                 /**< Agent identifier */
    float capability_score;            /**< Capability match [0-1] */
    float load_score;                  /**< Inverse load [0-1] */
    float energy_score;                /**< Energy level [0-1] */
    float locality_score;              /**< Proximity [0-1] */
    float deadline_score;              /**< Ability to meet deadline [0-1] */
    float total_score;                 /**< Weighted total score */
    bool is_capable;                   /**< Meets minimum requirements */
} swarm_agent_score_t;

/** Opaque scheduler type */
typedef struct swarm_task_scheduler swarm_task_scheduler_t;

//=============================================================================
// Scheduler API
//=============================================================================

/**
 * @brief Create a task scheduler
 *
 * @param task_manager Associated task manager
 * @param config Configuration (NULL for defaults)
 * @return Scheduler or NULL on error
 */
swarm_task_scheduler_t* swarm_scheduler_create(
    swarm_task_manager_t* task_manager,
    const swarm_scheduler_config_t* config
);

/**
 * @brief Destroy scheduler
 *
 * @param scheduler Scheduler to destroy
 */
void swarm_scheduler_destroy(swarm_task_scheduler_t* scheduler);

/**
 * @brief Get default scheduler configuration
 *
 * @param config Output configuration
 */
void swarm_scheduler_default_config(swarm_scheduler_config_t* config);

/**
 * @brief Get default hybrid weights
 *
 * @param weights Output weights
 */
void swarm_scheduler_default_weights(swarm_scheduler_weights_t* weights);

//=============================================================================
// Agent Management
//=============================================================================

/**
 * @brief Register an agent with the scheduler
 *
 * @param scheduler Task scheduler
 * @param profile Agent capability profile
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_register_agent(
    swarm_task_scheduler_t* scheduler,
    const swarm_agent_profile_t* profile
);

/**
 * @brief Unregister an agent
 *
 * @param scheduler Task scheduler
 * @param agent_id Agent to unregister
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_unregister_agent(
    swarm_task_scheduler_t* scheduler,
    uint32_t agent_id
);

/**
 * @brief Update agent profile
 *
 * @param scheduler Task scheduler
 * @param profile Updated profile
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_update_agent(
    swarm_task_scheduler_t* scheduler,
    const swarm_agent_profile_t* profile
);

/**
 * @brief Get agent's task queue
 *
 * @param scheduler Task scheduler
 * @param agent_id Agent identifier
 * @return Agent's task queue or NULL if not found
 */
swarm_task_queue_t* swarm_scheduler_get_agent_queue(
    swarm_task_scheduler_t* scheduler,
    uint32_t agent_id
);

/**
 * @brief Set agent availability
 *
 * @param scheduler Task scheduler
 * @param agent_id Agent identifier
 * @param is_available New availability
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_set_agent_available(
    swarm_task_scheduler_t* scheduler,
    uint32_t agent_id,
    bool is_available
);

//=============================================================================
// Scheduling API
//=============================================================================

/**
 * @brief Schedule a single task
 *
 * WHAT: Find best agent for a task and assign it
 * WHY:  Distribute work efficiently
 * HOW:  Apply scheduling algorithm to find best match
 *
 * @param scheduler Task scheduler
 * @param task_id Task to schedule
 * @param assigned_agent Output: agent task was assigned to
 * @return Schedule result
 */
swarm_schedule_result_t swarm_scheduler_schedule_task(
    swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    uint32_t* assigned_agent
);

/**
 * @brief Schedule all pending tasks
 *
 * @param scheduler Task scheduler
 * @param scheduled_count Output: number of tasks scheduled
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_schedule_all(
    swarm_task_scheduler_t* scheduler,
    uint32_t* scheduled_count
);

/**
 * @brief Score all agents for a task
 *
 * WHAT: Compute scheduling scores for all agents
 * WHY:  Visibility into scheduling decisions
 * HOW:  Apply algorithm to compute per-agent scores
 *
 * @param scheduler Task scheduler
 * @param task_id Task to score agents for
 * @param scores Output array (must hold MAX_AGENTS entries)
 * @param count Output: number of scores returned
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_score_agents(
    swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    swarm_agent_score_t* scores,
    uint32_t* count
);

/**
 * @brief Find best agent for a task (without assigning)
 *
 * @param scheduler Task scheduler
 * @param task_id Task to find agent for
 * @param best_agent Output: best agent ID
 * @param score Output: agent's score
 * @return Schedule result
 */
swarm_schedule_result_t swarm_scheduler_find_best_agent(
    swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    uint32_t* best_agent,
    float* score
);

/**
 * @brief Reassign a task to different agent
 *
 * @param scheduler Task scheduler
 * @param task_id Task to reassign
 * @param new_agent New agent ID
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_reassign_task(
    swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    uint32_t new_agent
);

/**
 * @brief Process periodic rescheduling
 *
 * Called periodically to re-evaluate and optimize task assignments
 *
 * @param scheduler Task scheduler
 * @return Number of tasks reassigned
 */
uint32_t swarm_scheduler_process(swarm_task_scheduler_t* scheduler);

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Set scheduling algorithm
 *
 * @param scheduler Task scheduler
 * @param algorithm New algorithm
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_set_algorithm(
    swarm_task_scheduler_t* scheduler,
    swarm_scheduler_algorithm_t algorithm
);

/**
 * @brief Get current scheduling algorithm
 *
 * @param scheduler Task scheduler
 * @return Current algorithm
 */
swarm_scheduler_algorithm_t swarm_scheduler_get_algorithm(
    const swarm_task_scheduler_t* scheduler
);

/**
 * @brief Set hybrid algorithm weights
 *
 * @param scheduler Task scheduler
 * @param weights New weights
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_set_weights(
    swarm_task_scheduler_t* scheduler,
    const swarm_scheduler_weights_t* weights
);

/**
 * @brief Get scheduler statistics
 *
 * @param scheduler Task scheduler
 * @param stats Output statistics
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_get_stats(
    const swarm_task_scheduler_t* scheduler,
    swarm_scheduler_stats_t* stats
);

/**
 * @brief Reset scheduler statistics
 *
 * @param scheduler Task scheduler
 */
void swarm_scheduler_reset_stats(swarm_task_scheduler_t* scheduler);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get number of registered agents
 *
 * @param scheduler Task scheduler
 * @return Number of agents
 */
uint32_t swarm_scheduler_agent_count(const swarm_task_scheduler_t* scheduler);

/**
 * @brief Get number of available agents
 *
 * @param scheduler Task scheduler
 * @return Number of available agents
 */
uint32_t swarm_scheduler_available_agent_count(
    const swarm_task_scheduler_t* scheduler
);

/**
 * @brief Get total pending tasks
 *
 * @param scheduler Task scheduler
 * @return Number of pending tasks
 */
uint32_t swarm_scheduler_pending_count(const swarm_task_scheduler_t* scheduler);

/**
 * @brief Get agents capable of executing a task
 *
 * @param scheduler Task scheduler
 * @param task_id Task to check
 * @param agent_ids Output array for capable agent IDs
 * @param max_agents Maximum agents to return
 * @param count Output: number of capable agents
 * @return 0 on success, error code otherwise
 */
int swarm_scheduler_get_capable_agents(
    const swarm_task_scheduler_t* scheduler,
    uint64_t task_id,
    uint32_t* agent_ids,
    uint32_t max_agents,
    uint32_t* count
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get algorithm name
 *
 * @param algorithm Scheduling algorithm
 * @return Human-readable algorithm name
 */
const char* swarm_scheduler_algorithm_name(swarm_scheduler_algorithm_t algorithm);

/**
 * @brief Get schedule result name
 *
 * @param result Schedule result
 * @return Human-readable result name
 */
const char* swarm_schedule_result_name(swarm_schedule_result_t result);

/**
 * @brief Check if agent meets task requirements
 *
 * @param profile Agent profile
 * @param requirements Task requirements
 * @return true if agent is capable, false otherwise
 */
bool swarm_scheduler_agent_meets_requirements(
    const swarm_agent_profile_t* profile,
    const swarm_task_requirements_t* requirements
);

/**
 * @brief Compute capability match score
 *
 * @param profile Agent profile
 * @param requirements Task requirements
 * @return Capability score [0-1]
 */
float swarm_scheduler_compute_capability_score(
    const swarm_agent_profile_t* profile,
    const swarm_task_requirements_t* requirements
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SWARM_TASK_SCHEDULER_H
