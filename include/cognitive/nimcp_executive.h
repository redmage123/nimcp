/**
 * @file nimcp_executive.h
 * @brief Executive functions module - task switching, planning, inhibition
 *
 * WHAT: Dorsolateral prefrontal cortex (DLPFC) executive control
 * WHY:  Enable goal-directed behavior, multi-tasking, and impulse control
 * HOW:  Task queue, attention allocation, inhibitory control
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

#ifndef NIMCP_EXECUTIVE_H
#define NIMCP_EXECUTIVE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>  // For FILE
#include "cognitive/nimcp_sleep_wake.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct brain_struct* brain_t;
typedef struct theory_of_mind_s* theory_of_mind_t;
typedef struct brain_immune_system brain_immune_system_t;

//=============================================================================
// Task Types
//=============================================================================

/**
 * @brief Task types that executive controller can manage
 */
typedef enum {
    TASK_TYPE_CLASSIFICATION,    /**< Multi-class classification task */
    TASK_TYPE_REGRESSION,        /**< Continuous value prediction */
    TASK_TYPE_SEQUENCE,          /**< Temporal sequence processing */
    TASK_TYPE_PLANNING,          /**< Multi-step goal planning */
    TASK_TYPE_REASONING,         /**< Logical reasoning */
    TASK_TYPE_MEMORY_RETRIEVAL,  /**< Recall from knowledge base */
    TASK_TYPE_CUSTOM             /**< User-defined task */
} task_type_t;

/**
 * @brief Task priority levels
 */
typedef enum {
    PRIORITY_LOW = 0,     /**< Background tasks */
    PRIORITY_NORMAL = 1,  /**< Default priority */
    PRIORITY_HIGH = 2,    /**< Important tasks */
    PRIORITY_URGENT = 3,  /**< Immediate attention required */
    PRIORITY_CRITICAL = 4 /**< Safety-critical tasks */
} task_priority_t;

/**
 * @brief Task status
 */
typedef enum {
    TASK_STATUS_PENDING,   /**< Queued, not started */
    TASK_STATUS_ACTIVE,    /**< Currently executing */
    TASK_STATUS_SUSPENDED, /**< Paused mid-execution */
    TASK_STATUS_COMPLETED, /**< Successfully finished */
    TASK_STATUS_FAILED,    /**< Execution failed */
    TASK_STATUS_ABORTED    /**< Manually cancelled */
} task_status_t;

/**
 * @brief Task descriptor
 */
typedef struct {
    uint32_t task_id;            /**< Unique task identifier */
    task_type_t type;            /**< Task type */
    task_priority_t priority;    /**< Execution priority */
    task_status_t status;        /**< Current status */

    char name[64];               /**< Human-readable name */
    void* context;               /**< Task-specific context */

    uint64_t created_ms;         /**< When task was created */
    uint64_t started_ms;         /**< When execution started (0 if not started) */
    uint64_t completed_ms;       /**< When execution finished (0 if not finished) */
    uint64_t deadline_ms;        /**< Deadline for completion (0 = no deadline) */

    uint32_t steps_total;        /**< Total steps in plan (0 if not a plan) */
    uint32_t steps_completed;    /**< Steps completed so far */
} task_descriptor_t;

//=============================================================================
// Executive Controller
//=============================================================================

/**
 * @brief Opaque executive controller handle
 */
typedef struct executive_controller executive_controller_t;

/**
 * @brief Executive controller configuration
 */
typedef struct {
    uint32_t max_tasks;               /**< Maximum queued tasks (default: 16) */
    float task_switch_cost_ms;        /**< Time penalty for switching (default: 200ms) */
    float inhibition_threshold;       /**< Threshold for inhibitory control (default: 0.7) */
    uint32_t max_plan_depth;          /**< Maximum planning depth (default: 10) */
    bool enable_task_prioritization;  /**< Enable priority-based scheduling */
    bool enable_deadline_checking;    /**< Enable deadline violations */
    bool enable_portia_integration;   /**< Enable Portia tier awareness (default: true) */

    // Theory of Mind integration (Phase 10.6.1)
    bool enable_tom_integration;      /**< Enable ToM-informed decision making (default: false) */
    uint32_t max_agent_models;        /**< Maximum agent models to track (default: 8) */

    // Brain Immune System integration (Phase 12.x)
    bool enable_immune_integration;   /**< Enable immune state modulation (default: false) */
    float immune_impairment_threshold; /**< Inflammation level that impairs function (default: 0.6) */
    float immune_critical_threshold;   /**< Critical inflammation requiring intervention (default: 0.85) */

    // Quantum planning integration
    bool enable_quantum_executive;    /**< Enable quantum-inspired planning (default: true) */
} executive_config_t;

/**
 * @brief Create executive controller with default configuration
 *
 * WHAT: Initialize executive control center
 * WHY:  Enable multi-tasking and goal-directed behavior
 * HOW:  Allocate task queue and control structures
 *
 * @return Executive controller handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (initialization)
 * MALLOC: Yes (controller structure + task queue)
 */
executive_controller_t* executive_create(void);

/**
 * @brief Create executive controller with custom configuration
 *
 * @param config Custom configuration
 * @return Executive controller handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (initialization)
 */
executive_controller_t* executive_create_custom(const executive_config_t* config);

/**
 * @brief Destroy executive controller
 *
 * @param exec Executive controller to destroy
 *
 * COMPLEXITY: O(n) where n = number of active tasks
 * THREAD-SAFE: No (caller must ensure exclusive access)
 */
void executive_destroy(executive_controller_t* exec);

//=============================================================================
// Task Management
//=============================================================================

/**
 * @brief Add task to queue
 *
 * WHAT: Register new task for execution
 * WHY:  Enable multi-task coordination
 * HOW:  Insert into priority queue based on priority and deadline
 *
 * @param exec Executive controller
 * @param task Task descriptor
 * @return Task ID (>0) on success, 0 on error
 *
 * COMPLEXITY: O(log n) for priority queue insertion
 * THREAD-SAFE: No
 */
uint32_t executive_add_task(executive_controller_t* exec, const task_descriptor_t* task);

/**
 * @brief Switch to different task
 *
 * WHAT: Change active task
 * WHY:  Support multi-tasking and interruption handling
 * HOW:  Suspend current task, activate new task, record switch cost
 *
 * @param exec Executive controller
 * @param task_id ID of task to switch to
 * @param current_time_ms Current time (for switch cost tracking)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(log n)
 * THREAD-SAFE: No
 * SIDE_EFFECT: Increments switch count, records latency
 */
bool executive_switch_task(executive_controller_t* exec, uint32_t task_id, uint64_t current_time_ms);

/**
 * @brief Get currently active task
 *
 * @param exec Executive controller
 * @return Active task descriptor or NULL if no active task
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
const task_descriptor_t* executive_get_active_task(executive_controller_t* exec);

/**
 * @brief Complete current task
 *
 * @param exec Executive controller
 * @param success true if successful, false if failed
 * @param current_time_ms Current time
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
bool executive_complete_task(executive_controller_t* exec, bool success, uint64_t current_time_ms);

//=============================================================================
// Inhibitory Control
//=============================================================================

/**
 * @brief Inhibit a response
 *
 * WHAT: Suppress prepotent/automatic responses
 * WHY:  Enable impulse control and ethical behavior
 * HOW:  Check salience against inhibition threshold
 *
 * @param exec Executive controller
 * @param response_salience How strong is the prepotent response [0,1]
 * @param reason Optional reason for inhibition (can be NULL)
 * @return true if response should be inhibited, false if allowed
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (stateless)
 * EXAMPLE:
 *   if (executive_should_inhibit(exec, 0.9f, "unethical")) {
 *       return; // Suppress unethical response
 *   }
 */
bool executive_should_inhibit(executive_controller_t* exec, float response_salience, const char* reason);

//=============================================================================
// Planning
//=============================================================================

/**
 * @brief Plan type
 */
typedef enum {
    PLAN_TYPE_SEQUENTIAL,   /**< Linear sequence of steps */
    PLAN_TYPE_BRANCHING,    /**< Conditional branching */
    PLAN_TYPE_HIERARCHICAL  /**< Nested sub-goals */
} plan_type_t;

/**
 * @brief Plan step
 */
typedef struct {
    char description[128];   /**< What to do */
    void* action_data;       /**< Action-specific data */
    uint32_t estimated_cost; /**< Estimated time/resources */
    bool is_critical;        /**< Must succeed for plan to work */
} plan_step_t;

/**
 * @brief Plan structure
 */
typedef struct {
    plan_type_t type;
    uint32_t num_steps;
    plan_step_t* steps;
    char goal[128];          /**< Overall goal description */
} plan_t;

/**
 * @brief Create plan to achieve goal
 *
 * WHAT: Decompose goal into action sequence
 * WHY:  Enable goal-directed behavior
 * HOW:  Forward search with cost estimation
 *
 * @param exec Executive controller
 * @param goal Goal description
 * @param max_steps Maximum plan length
 * @return Plan structure or NULL on error
 *
 * COMPLEXITY: O(b^d) where b=branching factor, d=max_steps
 * THREAD-SAFE: No
 * MALLOC: Yes (plan structure + steps)
 */
plan_t* executive_create_plan(executive_controller_t* exec, const char* goal, uint32_t max_steps);

/**
 * @brief Destroy plan
 *
 * @param plan Plan to destroy
 *
 * COMPLEXITY: O(n) where n = number of steps
 * THREAD-SAFE: Yes
 */
void executive_destroy_plan(plan_t* plan);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Executive function statistics
 */
typedef struct {
    uint32_t total_tasks;           /**< Total tasks processed */
    uint32_t completed_tasks;       /**< Successfully completed */
    uint32_t failed_tasks;          /**< Failed tasks */
    uint32_t aborted_tasks;         /**< Aborted tasks */

    uint32_t total_switches;        /**< Total task switches */
    float avg_switch_cost_ms;       /**< Average switching latency */

    uint32_t inhibitions;           /**< Total inhibited responses */
    float inhibition_rate;          /**< Inhibitions / total decisions */

    uint32_t plans_created;         /**< Total plans generated */
    float avg_plan_length;          /**< Average steps per plan */
} executive_stats_t;

/**
 * @brief Get executive function statistics
 *
 * @param exec Executive controller
 * @param stats Output statistics structure
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (read-only)
 */
bool executive_get_stats(executive_controller_t* exec, executive_stats_t* stats);

/**
 * @brief Reset statistics
 *
 * @param exec Executive controller
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void executive_reset_stats(executive_controller_t* exec);

//=============================================================================
// Error Handling
//=============================================================================

/**
 * @brief Get last error message
 *
 * @return Error message string (valid until next API call)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (thread-local storage)
 */
const char* executive_get_last_error(void);

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
 * @param exec Executive controller
 * @param file Open file handle for writing
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = number of tasks
 * THREAD-SAFE: No (caller must ensure exclusive access)
 */
bool executive_save(executive_controller_t* exec, FILE* file);

/**
 * @brief Load executive controller state from file
 *
 * WHAT: Deserialize executive controller state from binary file
 * WHY:  Restore saved task queue, statistics, and configuration
 * HOW:  Read version marker, validate, reconstruct state
 *
 * Note: Brain reference must be set separately via executive_set_brain()
 * Note: Task context pointers are not restored (set to NULL)
 *
 * @param file Open file handle for reading
 * @return Executive controller handle or NULL on error
 *
 * COMPLEXITY: O(n) where n = number of tasks
 * THREAD-SAFE: Yes (creates new instance)
 */
executive_controller_t* executive_load(FILE* file);

/**
 * @brief Set brain reference for loaded executive controller
 *
 * WHAT: Associate executive controller with brain for neuromodulation
 * WHY:  Loaded executive controllers need brain reference restored
 * HOW:  Set brain field in executive controller struct
 *
 * @param exec Executive controller
 * @param brain Brain handle
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void executive_set_brain(executive_controller_t* exec, brain_t brain);

/* Forward declaration for global workspace - must match nimcp_global_workspace.h */
#ifndef GLOBAL_WORKSPACE_T_DEFINED
#define GLOBAL_WORKSPACE_T_DEFINED
typedef struct global_workspace_struct* global_workspace_t;
#endif

/**
 * @brief Set global workspace for conscious broadcasting
 *
 * WHAT: Associate executive controller with global workspace
 * WHY:  Enable conscious decision broadcasting and workspace attention
 * HOW:  Store workspace reference and subscribe to broadcasts
 *
 * @param exec Executive controller
 * @param workspace Global workspace handle (NULL to disable)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void executive_set_workspace(executive_controller_t* exec, global_workspace_t* workspace);

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
 * BIOLOGY: Prefrontal cortex has limited capacity
 *
 * @param exec Executive controller
 * @return Cognitive load [0, 1] (0=idle, 1=saturated)
 */
float executive_get_cognitive_load(executive_controller_t* exec);

/**
 * @brief Boost task priority based on external signal
 *
 * WHAT: Allow modules to boost task priority
 * WHY:  Curiosity-driven tasks should be prioritized when informative
 * HOW:  Increase priority by boost_amount
 *
 * @param exec Executive controller
 * @param task_name Task name to boost
 * @param boost_amount Priority boost [0, 1]
 * @return true if task found and boosted
 */
bool executive_boost_task_priority(executive_controller_t* exec,
                                    const char* task_name,
                                    float boost_amount);

//=============================================================================
// Portia Integration (Phase 11.5)
//=============================================================================

/**
 * @brief Get current Portia tier from executive controller
 *
 * WHAT: Query Portia tier state for external monitoring
 * WHY:  Allow other modules to check resource constraints
 * HOW:  Return cached tier value from Portia integration
 *
 * @param exec Executive controller
 * @return Current platform tier, or TIER_UNKNOWN if Portia disabled
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
uint32_t executive_get_portia_tier(executive_controller_t* exec);

/**
 * @brief Check if executive is in resource-aware mode
 *
 * WHAT: Query whether executive is adapting to resource constraints
 * WHY:  Diagnostic and status reporting
 * HOW:  Return resource_aware_mode flag
 *
 * @param exec Executive controller
 * @return true if resource-aware mode active
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool executive_is_resource_aware(executive_controller_t* exec);

/**
 * @brief Get recommended max plan depth for current resources
 *
 * WHAT: Query planning depth adjusted for current tier
 * WHY:  Allow external planners to adapt complexity
 * HOW:  Scale max_plan_depth based on current tier
 *
 * BIOLOGY: Prefrontal cortex planning depth decreases under stress/fatigue
 *
 * @param exec Executive controller
 * @return Recommended max plan depth
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
uint32_t executive_get_recommended_plan_depth(executive_controller_t* exec);

/**
 * @brief Process pending bio-async messages for executive
 *
 * WHAT: Process incoming messages from bio-router inbox
 * WHY:  Receive Portia tier changes and other async events
 * HOW:  Call bio_router_process_inbox for executive's context
 *
 * @param exec Executive controller
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 *
 * COMPLEXITY: O(n) where n = number of messages
 * THREAD-SAFE: No
 */
uint32_t executive_process_messages(executive_controller_t* exec, uint32_t max_messages);

//=============================================================================
// Theory of Mind Integration (Phase 10.6.1)
//=============================================================================

/**
 * @brief Set Theory of Mind module for agent-aware decision making
 *
 * WHAT: Associate executive controller with ToM for social reasoning
 * WHY:  Enable consideration of other agents' beliefs and intentions
 * HOW:  Store ToM reference, enable query functions
 *
 * @param exec Executive controller
 * @param tom Theory of Mind module (can be NULL to disable)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void executive_set_theory_of_mind(executive_controller_t* exec, theory_of_mind_t tom);

/**
 * @brief Query agent-aware decision for a proposed action
 *
 * WHAT: Consider other agents' likely responses before acting
 * WHY:  Enable socially intelligent, coordinated behavior
 * HOW:  Use ToM to simulate agent responses, adjust decision
 *
 * @param exec Executive controller
 * @param action_description What action is being considered
 * @param affected_agent_ids Array of agent IDs that would be affected
 * @param num_affected_agents Number of affected agents
 * @param decision_output Output: recommended decision (buffer >= 512 bytes)
 * @param confidence Output: confidence in this decision [0.0, 1.0]
 * @return true if decision generated
 *
 * USAGE: Call before executing actions that affect other agents
 * COMPLEXITY: O(n) where n = num_affected_agents
 */
bool executive_query_agent_aware_decision(executive_controller_t* exec,
                                           const char* action_description,
                                           const uint32_t* affected_agent_ids,
                                           uint32_t num_affected_agents,
                                           char* decision_output,
                                           float* confidence);

/**
 * @brief Evaluate false belief scenarios for planning
 *
 * WHAT: Consider agent false beliefs when planning actions
 * WHY:  Agents may act on incorrect beliefs, need to account for this
 * HOW:  Query ToM for false beliefs, adjust plan accordingly
 *
 * @param exec Executive controller
 * @param agent_id Which agent to check
 * @param has_false_beliefs Output: whether agent has false beliefs
 * @param false_belief_description Output: description (buffer >= 256 bytes)
 * @return true if evaluation succeeded
 *
 * COMPLEXITY: O(1)
 */
bool executive_check_agent_false_beliefs(executive_controller_t* exec,
                                          uint32_t agent_id,
                                          bool* has_false_beliefs,
                                          char* false_belief_description);

/**
 * @brief Model agent intentions for coordination
 *
 * WHAT: Get prediction of what agent plans to do
 * WHY:  Enable proactive coordination and conflict avoidance
 * HOW:  Query ToM for agent's current intentions
 *
 * @param exec Executive controller
 * @param agent_id Which agent to query
 * @param intention_output Output: predicted intention (buffer >= 256 bytes)
 * @param likelihood Output: confidence in prediction [0.0, 1.0]
 * @return true if intention retrieved
 *
 * COMPLEXITY: O(1)
 */
bool executive_model_agent_intentions(executive_controller_t* exec,
                                       uint32_t agent_id,
                                       char* intention_output,
                                       float* likelihood);

//=============================================================================
// Brain Immune System Integration (Phase 12.x)
//=============================================================================

/**
 * @brief Set brain immune system for executive function modulation
 *
 * WHAT: Associate executive controller with immune system for inflammation-based modulation
 * WHY:  High inflammation (cytokines) impairs executive function (cognitive fog)
 * HOW:  Store immune system reference, enable inflammation-based adjustments
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines (IL-1, IL-6, TNF-α) impair prefrontal function
 * - "Sickness behavior" reduces executive capacity during immune response
 * - Cognitive fog manifests as: slower task switching, reduced inhibition, simpler planning
 *
 * @param exec Executive controller
 * @param immune Brain immune system (can be NULL to disable)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void executive_set_immune_system(executive_controller_t* exec, brain_immune_system_t* immune);

/**
 * @brief Get current inflammation-adjusted cognitive capacity
 *
 * WHAT: Query executive capacity adjusted for immune-induced impairment
 * WHY:  High inflammation reduces cognitive resources
 * HOW:  Scale capacity by inflammation level (higher inflammation = lower capacity)
 *
 * BIOLOGY: Cytokine-induced cognitive fog reduces working memory, attention, processing speed
 *
 * @param exec Executive controller
 * @return Adjusted capacity [0, 1] (1=full capacity, 0=completely impaired)
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float executive_get_immune_adjusted_capacity(executive_controller_t* exec);

/**
 * @brief Check if executive function is significantly impaired
 *
 * WHAT: Determine if inflammation has crossed impairment threshold
 * WHY:  System may need to reduce cognitive load or alert operator
 * HOW:  Compare current inflammation to impairment threshold
 *
 * @param exec Executive controller
 * @return true if executive function significantly impaired
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool executive_is_immune_impaired(executive_controller_t* exec);

/**
 * @brief Get inflammation-adjusted task switch cost
 *
 * WHAT: Calculate task switching cost adjusted for inflammation
 * WHY:  Inflammation increases cognitive rigidity and switch cost
 * HOW:  Scale base switch cost by inflammation level
 *
 * BIOLOGY: Cytokines increase perseveration and reduce cognitive flexibility
 *
 * @param exec Executive controller
 * @return Adjusted switch cost in milliseconds
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float executive_get_immune_adjusted_switch_cost(executive_controller_t* exec);

/**
 * @brief Get inflammation-adjusted inhibition threshold
 *
 * WHAT: Calculate inhibition threshold adjusted for inflammation
 * WHY:  High inflammation impairs impulse control
 * HOW:  Increase threshold (harder to inhibit) with inflammation
 *
 * BIOLOGY: Pro-inflammatory states reduce prefrontal inhibitory control
 *
 * @param exec Executive controller
 * @return Adjusted inhibition threshold [0, 1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
float executive_get_immune_adjusted_inhibition(executive_controller_t* exec);

//=============================================================================
// Sleep Integration
//=============================================================================

/**
 * @brief Set current sleep state for executive modulation
 *
 * WHAT: Update sleep state and apply modulation factors
 * WHY:  Executive function is highly sensitive to sleep state
 * HOW:  Store state, modulate inhibition/flexibility/switch cost
 *
 * BIOLOGICAL BASIS:
 * - AWAKE: Full executive control
 * - DROWSY: Impaired inhibition, reduced flexibility
 * - NREM: Executive functions offline (PFC recovery)
 * - REM: Reduced executive control (dream bizarreness)
 *
 * @param exec Executive controller
 * @param state Current sleep state
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No
 */
void executive_set_sleep_state(executive_controller_t* exec, sleep_state_t state);

//=============================================================================
// Monte Carlo Integration API
//=============================================================================

/**
 * @brief Select task using epsilon-greedy strategy with MC sampling
 *
 * WHAT: Choose task with exploration-exploitation trade-off
 * WHY:  Enable adaptive task selection that explores sub-optimal options
 * HOW:  With probability epsilon, pick random task; else pick best
 *
 * BIOLOGICAL BASIS:
 * - Dopaminergic exploration vs exploitation trade-off
 * - Random exploration when uncertainty is high
 *
 * @param exec Executive controller
 * @param epsilon Exploration probability [0, 1]
 * @return Selected task or NULL if no tasks
 *
 * COMPLEXITY: O(n) where n = number of pending tasks
 * THREAD-SAFE: No
 */
task_descriptor_t* executive_select_task_epsilon_greedy_mc(
    executive_controller_t* exec,
    float epsilon);

/**
 * @brief Estimate task value via Monte Carlo rollouts
 *
 * WHAT: Estimate expected value of completing a task
 * WHY:  Enable informed task selection based on future rewards
 * HOW:  Simulate task completion trajectories, average outcomes
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal value estimation via mental simulation
 * - Model-based reinforcement learning
 *
 * @param exec Executive controller
 * @param task Task to evaluate
 * @param num_rollouts Number of MC simulations
 * @param discount Temporal discount factor [0, 1]
 * @return Estimated task value
 *
 * COMPLEXITY: O(num_rollouts * avg_rollout_length)
 * THREAD-SAFE: No
 */
float executive_estimate_task_value_mc(
    executive_controller_t* exec,
    const task_descriptor_t* task,
    uint32_t num_rollouts,
    float discount);

/**
 * @brief Select task using softmax with MC value estimation
 *
 * WHAT: Probabilistically select task based on estimated values
 * WHY:  Softer exploration than epsilon-greedy, respects value estimates
 * HOW:  Compute values via MC, apply softmax, sample
 *
 * BIOLOGICAL BASIS:
 * - Temperature modulates exploration/exploitation
 * - Low temperature = exploit best, high temperature = explore randomly
 *
 * @param exec Executive controller
 * @param temperature Softmax temperature (higher = more random)
 * @param num_rollouts MC rollouts per task for value estimation
 * @return Selected task or NULL if no tasks
 *
 * COMPLEXITY: O(n * num_rollouts)
 * THREAD-SAFE: No
 */
task_descriptor_t* executive_select_task_softmax_mc(
    executive_controller_t* exec,
    float temperature,
    uint32_t num_rollouts);

/**
 * @brief Get thread-local MC seed for executive module
 *
 * WHAT: Access thread-local RNG seed
 * WHY:  Allow external seeding for reproducibility
 * HOW:  Return pointer to thread-local seed variable
 *
 * @return Pointer to thread-local seed
 */
uint32_t* executive_get_mc_seed(void);

//=============================================================================
// MCTS-Based Goal Decomposition API
//=============================================================================

/**
 * @brief MCTS planning configuration
 */
typedef struct {
    uint32_t max_iterations;          /**< MCTS iterations (default: 100) */
    uint32_t max_depth;               /**< Maximum planning depth (default: 10) */
    float exploration_constant;       /**< UCB1 exploration (default: 1.414) */
    float discount_factor;            /**< Future reward discount (default: 0.95) */
    uint32_t rollout_depth;           /**< Simulation rollout depth (default: 5) */
    bool enable_pruning;              /**< Prune low-value branches (default: true) */
    float pruning_threshold;          /**< Value threshold for pruning (default: 0.1) */
} executive_mcts_config_t;

/**
 * @brief MCTS planning result statistics
 */
typedef struct {
    uint32_t nodes_expanded;          /**< Total tree nodes created */
    uint32_t iterations_run;          /**< Actual MCTS iterations completed */
    uint32_t max_depth_reached;       /**< Deepest node in search tree */
    float root_value;                 /**< Estimated value at root */
    float planning_time_ms;           /**< Time spent planning */
    float avg_branching_factor;       /**< Average branching factor */
} executive_mcts_stats_t;

/**
 * @brief Initialize MCTS config with defaults
 *
 * @param config Config struct to initialize
 */
void executive_mcts_config_init(executive_mcts_config_t* config);

/**
 * @brief Create plan using MCTS-guided goal decomposition
 *
 * WHAT: Decompose goal into action sequence using MCTS
 * WHY:  Optimal multi-step planning with uncertainty handling
 * HOW:  Build search tree with UCB1 selection, rollout simulation
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex mental simulation and evaluation
 * - Tree-structured exploration of action spaces
 * - Value-guided search with uncertainty (dopamine-like)
 *
 * ALGORITHM:
 * 1. Selection: Follow UCB1 to promising nodes
 * 2. Expansion: Add child nodes for unexplored actions
 * 3. Simulation: Random rollout to estimate value
 * 4. Backpropagation: Update node statistics
 *
 * @param exec Executive controller
 * @param goal Goal description
 * @param config MCTS configuration (NULL for defaults)
 * @param stats Output statistics (can be NULL)
 * @return Plan structure or NULL on error
 *
 * COMPLEXITY: O(iterations * depth * branching_factor)
 * THREAD-SAFE: No
 * MALLOC: Yes (plan structure + steps)
 */
plan_t* executive_create_plan_mcts(
    executive_controller_t* exec,
    const char* goal,
    const executive_mcts_config_t* config,
    executive_mcts_stats_t* stats);

/**
 * @brief Evaluate plan quality using MCTS value estimation
 *
 * WHAT: Estimate expected success probability of a plan
 * WHY:  Compare alternative plans, validate existing plans
 * HOW:  Monte Carlo rollouts from plan execution
 *
 * @param exec Executive controller
 * @param plan Plan to evaluate
 * @param num_rollouts Number of MC simulations
 * @return Estimated success probability [0, 1]
 *
 * COMPLEXITY: O(num_rollouts * plan_length)
 * THREAD-SAFE: No
 */
float executive_evaluate_plan_mcts(
    executive_controller_t* exec,
    const plan_t* plan,
    uint32_t num_rollouts);

/**
 * @brief Re-plan from current step using MCTS
 *
 * WHAT: Adapt plan when conditions change mid-execution
 * WHY:  Handle unexpected outcomes, dynamic environments
 * HOW:  MCTS from current state with remaining goal
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal flexibility and plan adaptation
 * - Error-driven replanning (prediction error)
 *
 * @param exec Executive controller
 * @param current_plan Plan being executed
 * @param current_step Step index where replanning starts
 * @param config MCTS configuration (NULL for defaults)
 * @return New plan from current_step onwards, or NULL on error
 *
 * COMPLEXITY: O(iterations * (max_steps - current_step))
 * THREAD-SAFE: No
 */
plan_t* executive_replan_mcts(
    executive_controller_t* exec,
    const plan_t* current_plan,
    uint32_t current_step,
    const executive_mcts_config_t* config);

/**
 * @brief Get best action from MCTS without full plan
 *
 * WHAT: Quick single-action decision via MCTS
 * WHY:  When full plan not needed, faster decision
 * HOW:  Run MCTS, return highest-visit child action
 *
 * @param exec Executive controller
 * @param goal Current goal context
 * @param config MCTS configuration (NULL for defaults)
 * @param action_value Output: estimated value of action (can be NULL)
 * @return Best action description or NULL on error
 *
 * NOTE: Returned string is allocated, caller must free
 *
 * COMPLEXITY: O(iterations * depth)
 * THREAD-SAFE: No
 */
char* executive_get_best_action_mcts(
    executive_controller_t* exec,
    const char* goal,
    const executive_mcts_config_t* config,
    float* action_value);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_H */
