//=============================================================================
// nimcp_procedural.h - Procedural Memory System for Skills, Habits, and Actions
//=============================================================================
/**
 * @file nimcp_procedural.h
 * @brief Memory for skills, habits, and automated behavioral sequences
 *
 * WHAT: Procedural memory system for storing and executing learned behaviors
 * WHY:  Enable skill acquisition, habit formation, and automated action sequences
 * HOW:  Motor programs with multi-stage skill encoding and stimulus-response habits
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Procedural Memory Types:
 *   +-----------------------------------------------------------------------+
 *   |  The brain maintains several types of procedural knowledge:          |
 *   |                                                                       |
 *   |  MOTOR PROGRAMS:                                                      |
 *   |  - Coordinated action sequences (walking, typing, playing piano)     |
 *   |  - Primary motor cortex (M1), supplementary motor area (SMA)         |
 *   |  - Cerebellum for timing and coordination                            |
 *   |  - Basal ganglia for action selection and sequencing                 |
 *   |                                                                       |
 *   |  COGNITIVE SKILLS:                                                    |
 *   |  - Mental procedures (mental arithmetic, reading, problem-solving)   |
 *   |  - Prefrontal cortex + basal ganglia procedural system               |
 *   |  - Become automatic with practice (less PFC involvement)             |
 *   |                                                                       |
 *   |  PERCEPTUAL SKILLS:                                                   |
 *   |  - Pattern recognition (face recognition, wine tasting)              |
 *   |  - Occipital/temporal cortex with basal ganglia gating               |
 *   |  - Priming effects from prior exposure                               |
 *   |                                                                       |
 *   |  HABITS:                                                              |
 *   |  - Automatic stimulus-response associations                          |
 *   |  - Dorsolateral striatum (habit region)                             |
 *   |  - Dopaminergic reinforcement learning                               |
 *   +-----------------------------------------------------------------------+
 *
 *   Skill Acquisition Stages (Fitts & Posner, 1967):
 *   +-----------------------------------------------------------------------+
 *   |  COGNITIVE STAGE:                                                     |
 *   |  - Explicit, effortful, step-by-step execution                       |
 *   |  - High working memory load                                          |
 *   |  - Errors common, slow performance                                   |
 *   |  - Heavy prefrontal cortex involvement                               |
 *   |                                                                       |
 *   |  ASSOCIATIVE STAGE:                                                   |
 *   |  - Practice-based refinement                                         |
 *   |  - Errors decrease, speed increases                                  |
 *   |  - Chunks form, reducing WM load                                     |
 *   |  - Shift toward basal ganglia control                                |
 *   |                                                                       |
 *   |  AUTONOMOUS STAGE:                                                    |
 *   |  - Automatic, low attention required                                 |
 *   |  - Can perform while doing other tasks                               |
 *   |  - Resistant to verbal interference                                  |
 *   |  - Fully proceduralized in basal ganglia                             |
 *   +-----------------------------------------------------------------------+
 *
 *   Habit Formation Neural Circuitry:
 *   +-----------------------------------------------------------------------+
 *   |  Habit Loop: Cue -> Routine -> Reward                                |
 *   |                                                                       |
 *   |  CUE DETECTION:                                                       |
 *   |  - Sensory cortex identifies trigger                                 |
 *   |  - Dopamine signals salience                                         |
 *   |                                                                       |
 *   |  ROUTINE EXECUTION:                                                   |
 *   |  - Dorsolateral striatum activates chunk                             |
 *   |  - Motor/cognitive routines execute                                  |
 *   |                                                                       |
 *   |  REWARD PREDICTION:                                                   |
 *   |  - Ventral striatum computes reward prediction error                 |
 *   |  - Dopamine strengthens or weakens connection                        |
 *   |                                                                       |
 *   |  CHUNKING:                                                            |
 *   |  - Start/end markers bracket action sequence                         |
 *   |  - Interior steps become automatic                                   |
 *   |  - Attention only needed at boundaries                               |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Skill creation: O(n) where n = number of steps
 * - Practice update: O(1) per practice
 * - Execution: O(steps) with O(1) per step
 * - Habit trigger check: O(1) signature comparison
 * - Memory: ~512 bytes per skill + steps, ~128 bytes per habit
 *
 * INTEGRATION:
 * - Core: PR Memory nodes for skill/habit storage
 * - Entanglement: Link related skills and habits
 * - Theta-Gamma: Practice during consolidation cycles
 * - Executive: Skill selection and execution monitoring
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PROCEDURAL_H
#define NIMCP_PROCEDURAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Dependencies
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_prime_signature.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_entanglement.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum number of skills per procedural memory system */
#define PROC_MAX_SKILLS                     1024

/** Maximum number of habits per procedural memory system */
#define PROC_MAX_HABITS                     512

/** Maximum steps per skill */
#define PROC_MAX_STEPS_PER_SKILL            64

/** Maximum sub-skills per chunk */
#define PROC_MAX_SUB_SKILLS                 16

/** Maximum action description length */
#define PROC_MAX_ACTION_DESCRIPTION         256

/** Maximum skill name length */
#define PROC_MAX_SKILL_NAME                 128

/** Maximum reward history length for habits */
#define PROC_MAX_REWARD_HISTORY             100

/** Practice count threshold for cognitive -> associative transition */
#define PROC_COGNITIVE_PRACTICE_THRESHOLD   50

/** Practice count threshold for associative -> autonomous transition */
#define PROC_ASSOCIATIVE_PRACTICE_THRESHOLD 500

/** Accuracy threshold for cognitive -> associative transition */
#define PROC_COGNITIVE_ACCURACY_THRESHOLD   0.7f

/** Accuracy threshold for associative -> autonomous transition */
#define PROC_ASSOCIATIVE_ACCURACY_THRESHOLD 0.9f

/** Automaticity threshold for cognitive -> associative transition */
#define PROC_COGNITIVE_AUTO_THRESHOLD       0.3f

/** Automaticity threshold for associative -> autonomous transition */
#define PROC_ASSOCIATIVE_AUTO_THRESHOLD     0.7f

/** Default decay rate per day for unpracticed skills */
#define PROC_DEFAULT_DECAY_RATE             0.05f

/** Minimum skill strength before degradation */
#define PROC_MIN_SKILL_STRENGTH             0.1f

/** Default habit strength initial value */
#define PROC_DEFAULT_HABIT_STRENGTH         0.5f

/** Habit trigger similarity threshold */
#define PROC_HABIT_TRIGGER_THRESHOLD        0.7f

/** Invalid ID sentinel */
#define PROC_INVALID_ID                     UINT64_MAX

/** Epsilon for floating-point comparisons */
#define PROC_EPSILON                        1e-6f

//=============================================================================
// Type Definitions - Enumerations
//=============================================================================

/**
 * @brief Skill acquisition stage (Fitts & Posner model)
 *
 * WHAT: Current learning stage of a skill
 * WHY:  Different stages require different processing and resources
 * HOW:  Progresses through practice, accuracy, and automaticity thresholds
 */
typedef enum {
    SKILL_STAGE_COGNITIVE = 0,    /**< Explicit, effortful, step-by-step */
    SKILL_STAGE_ASSOCIATIVE,      /**< Practice-based, reducing errors */
    SKILL_STAGE_AUTONOMOUS        /**< Automatic, low attention required */
} skill_stage_t;

/**
 * @brief Procedural memory type
 *
 * WHAT: Category of procedural knowledge
 * WHY:  Different types have different processing characteristics
 * HOW:  Determines execution pathway and resource requirements
 */
typedef enum {
    PROC_MOTOR = 0,               /**< Motor skill (physical actions) */
    PROC_COGNITIVE,               /**< Cognitive skill (mental operations) */
    PROC_PERCEPTUAL,              /**< Perceptual skill (pattern recognition) */
    PROC_HABIT                    /**< Stimulus-response habit */
} procedural_type_t;

/**
 * @brief Skill execution status
 *
 * WHAT: Current state of skill execution
 * WHY:  Track progress through execution sequence
 */
typedef enum {
    PROC_EXEC_IDLE = 0,           /**< Not executing */
    PROC_EXEC_STARTING,           /**< Beginning execution */
    PROC_EXEC_RUNNING,            /**< Mid-execution */
    PROC_EXEC_PAUSED,             /**< Temporarily paused */
    PROC_EXEC_COMPLETING,         /**< Finishing execution */
    PROC_EXEC_COMPLETED,          /**< Successfully completed */
    PROC_EXEC_FAILED              /**< Execution failed */
} proc_exec_status_t;

/**
 * @brief Error codes for procedural memory operations
 */
typedef enum {
    PROC_SUCCESS = 0,                   /**< Operation succeeded */
    PROC_ERROR_NULL_POINTER = -1,       /**< NULL argument provided */
    PROC_ERROR_INVALID_TYPE = -2,       /**< Invalid procedural type */
    PROC_ERROR_INVALID_STAGE = -3,      /**< Invalid skill stage */
    PROC_ERROR_NO_MEMORY = -4,          /**< Memory allocation failed */
    PROC_ERROR_CAPACITY = -5,           /**< Maximum skills/habits reached */
    PROC_ERROR_NOT_FOUND = -6,          /**< Skill/habit ID not found */
    PROC_ERROR_INVALID_STEP = -7,       /**< Invalid step configuration */
    PROC_ERROR_MAX_STEPS = -8,          /**< Maximum steps exceeded */
    PROC_ERROR_ALREADY_EXECUTING = -9,  /**< Skill already being executed */
    PROC_ERROR_NOT_EXECUTING = -10,     /**< No skill currently executing */
    PROC_ERROR_INVALID_CHUNK = -11,     /**< Invalid chunk configuration */
    PROC_ERROR_CIRCULAR_CHUNK = -12,    /**< Circular chunking detected */
    PROC_ERROR_INTERNAL = -13           /**< Internal error */
} procedural_error_t;

//=============================================================================
// Type Definitions - Structures
//=============================================================================

/**
 * @brief Single step in a procedural skill
 *
 * WHAT: One action in an action sequence
 * WHY:  Skills are composed of ordered steps
 * HOW:  Step with preconditions, action, and postconditions
 *
 * Memory layout: ~200 bytes (excluding description allocation)
 */
typedef struct {
    uint64_t step_id;                    /**< Unique step identifier */
    char* action_description;            /**< Human-readable action description */
    prime_signature_t action_signature;  /**< Signature of this action */
    float duration_ms;                   /**< Expected duration in milliseconds */
    float precision_required;            /**< Required precision [0, 1] */

    // Preconditions and effects
    prime_signature_t preconditions;     /**< Required state before step */
    prime_signature_t postconditions;    /**< Expected state after step */

    // Execution tracking
    size_t execution_count;              /**< How many times executed */
    float success_rate;                  /**< Historical success rate */
    float mean_duration_ms;              /**< Mean actual duration */

    // Flags
    bool is_optional;                    /**< Can be skipped */
    bool requires_attention;             /**< Needs conscious attention */
} procedural_step_t;

/**
 * @brief Procedural skill structure
 *
 * WHAT: Complete representation of a learned skill
 * WHY:  Store all information for skill execution and learning
 * HOW:  Identity, steps, state, and learning metrics
 *
 * Memory layout: ~512 bytes (excluding steps and description allocations)
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t skill_id;                   /**< Unique skill identifier */
    char* skill_name;                    /**< Human-readable skill name */
    procedural_type_t type;              /**< Skill type (motor/cognitive/perceptual) */
    skill_stage_t stage;                 /**< Current acquisition stage */

    //-------------------------------------------------------------------------
    // Action Sequence
    //-------------------------------------------------------------------------
    procedural_step_t* steps;            /**< Array of steps */
    size_t num_steps;                    /**< Number of steps in sequence */
    size_t allocated_steps;              /**< Allocated capacity for steps */

    //-------------------------------------------------------------------------
    // PR Integration
    //-------------------------------------------------------------------------
    prime_signature_t skill_signature;   /**< Content signature of skill */
    nimcp_quaternion_t skill_quaternion; /**< Semantic state */
    pr_memory_node_t* memory_node;       /**< Underlying PR memory storage */

    //-------------------------------------------------------------------------
    // Acquisition State
    //-------------------------------------------------------------------------
    size_t practice_count;               /**< Total practice sessions */
    float accuracy;                      /**< Current accuracy [0, 1] */
    float speed;                         /**< Execution speed factor (1.0 = normal) */
    float automaticity;                  /**< How automatic [0, 1] */
    float strength;                      /**< Overall skill strength [0, 1] */

    //-------------------------------------------------------------------------
    // Learning History
    //-------------------------------------------------------------------------
    float* accuracy_history;             /**< Recent accuracy values */
    size_t history_len;                  /**< Length of history array */
    float learning_rate;                 /**< Current learning rate */
    float last_practice_time;            /**< Time of last practice */

    //-------------------------------------------------------------------------
    // Chunking
    //-------------------------------------------------------------------------
    uint64_t parent_skill_id;            /**< Parent if this is a sub-skill */
    uint64_t* sub_skills;                /**< Component sub-skill IDs */
    size_t num_sub_skills;               /**< Number of sub-skills */
    bool is_chunk;                       /**< True if this is a chunk */

    //-------------------------------------------------------------------------
    // Execution State
    //-------------------------------------------------------------------------
    proc_exec_status_t exec_status;      /**< Current execution status */
    size_t current_step_index;           /**< Current step during execution */
    float exec_start_time;               /**< When execution started */

    //-------------------------------------------------------------------------
    // Temporal Information
    //-------------------------------------------------------------------------
    float creation_time;                 /**< When skill was created */
    float total_practice_time;           /**< Total time spent practicing */

} procedural_skill_t;

/**
 * @brief Stimulus-response habit
 *
 * WHAT: Automatic association between cue and response
 * WHY:  Model habitual behaviors triggered by context
 * HOW:  Cue signature matches trigger response
 *
 * Memory layout: ~320 bytes (excluding history allocation)
 */
typedef struct {
    //-------------------------------------------------------------------------
    // Identity
    //-------------------------------------------------------------------------
    uint64_t habit_id;                   /**< Unique habit identifier */

    //-------------------------------------------------------------------------
    // Stimulus-Response Association
    //-------------------------------------------------------------------------
    prime_signature_t cue_signature;     /**< Trigger cue signature */
    prime_signature_t response_signature;/**< Automatic response signature */
    nimcp_quaternion_t cue_state;        /**< Expected cue state */
    nimcp_quaternion_t response_state;   /**< Response execution state */

    //-------------------------------------------------------------------------
    // Habit Strength
    //-------------------------------------------------------------------------
    float strength;                      /**< Habit strength [0, 1] */
    float context_dependency;            /**< How context-specific [0, 1] */
    float automaticity;                  /**< How automatic [0, 1] */

    //-------------------------------------------------------------------------
    // Reward Learning
    //-------------------------------------------------------------------------
    float* reward_history;               /**< Recent reward values */
    size_t history_len;                  /**< Length of reward history */
    size_t history_capacity;             /**< Capacity of reward history */
    float mean_reward;                   /**< Running mean reward */
    float reward_prediction;             /**< Current reward prediction */

    //-------------------------------------------------------------------------
    // Execution Statistics
    //-------------------------------------------------------------------------
    size_t trigger_count;                /**< How many times triggered */
    size_t execution_count;              /**< How many times executed */
    float last_trigger_time;             /**< Time of last trigger */
    float last_execution_time;           /**< Time of last execution */

    //-------------------------------------------------------------------------
    // PR Integration
    //-------------------------------------------------------------------------
    pr_memory_node_t* memory_node;       /**< Underlying PR memory storage */

    //-------------------------------------------------------------------------
    // Linked Skill (optional)
    //-------------------------------------------------------------------------
    uint64_t linked_skill_id;            /**< Associated skill ID (if any) */
    bool has_linked_skill;               /**< Whether a skill is linked */

} procedural_habit_t;

/**
 * @brief Configuration for procedural memory system
 *
 * WHAT: Parameters controlling system behavior
 * WHY:  Allow tuning for different use cases
 * HOW:  Set at creation, some modifiable at runtime
 */
typedef struct {
    size_t max_skills;                   /**< Maximum skills to store */
    size_t max_habits;                   /**< Maximum habits to store */
    size_t max_steps_per_skill;          /**< Maximum steps per skill */
    size_t max_sub_skills;               /**< Maximum sub-skills per chunk */

    // Learning parameters
    float base_learning_rate;            /**< Base learning rate */
    float skill_decay_rate;              /**< Decay rate for unpracticed skills */
    float habit_decay_rate;              /**< Decay rate for unexecuted habits */

    // Stage transition thresholds
    float cognitive_practice_threshold;  /**< Practice count for stage 1->2 */
    float associative_practice_threshold;/**< Practice count for stage 2->3 */
    float cognitive_accuracy_threshold;  /**< Accuracy for stage 1->2 */
    float associative_accuracy_threshold;/**< Accuracy for stage 2->3 */
    float cognitive_auto_threshold;      /**< Automaticity for stage 1->2 */
    float associative_auto_threshold;    /**< Automaticity for stage 2->3 */

    // Habit parameters
    float habit_trigger_threshold;       /**< Minimum similarity to trigger habit */
    float habit_initial_strength;        /**< Initial habit strength */
    float habit_reinforcement_rate;      /**< Rate of habit strengthening */

    // History tracking
    size_t accuracy_history_len;         /**< Length of accuracy history */
    size_t reward_history_len;           /**< Length of reward history */

} procedural_config_t;

/**
 * @brief Statistics for procedural memory system
 *
 * WHAT: Operational metrics and performance data
 * WHY:  Monitoring, debugging, analysis
 */
typedef struct {
    // Skill statistics
    size_t total_skills;                 /**< Total skills stored */
    size_t skills_cognitive;             /**< Skills in cognitive stage */
    size_t skills_associative;           /**< Skills in associative stage */
    size_t skills_autonomous;            /**< Skills in autonomous stage */
    size_t total_skill_executions;       /**< Total skill executions */
    float mean_skill_accuracy;           /**< Mean accuracy across skills */
    float mean_skill_automaticity;       /**< Mean automaticity across skills */

    // Habit statistics
    size_t total_habits;                 /**< Total habits stored */
    size_t total_habit_triggers;         /**< Total habit trigger checks */
    size_t total_habit_fires;            /**< Total habit activations */
    float mean_habit_strength;           /**< Mean habit strength */
    float mean_habit_reward;             /**< Mean reward across habits */

    // Chunking statistics
    size_t total_chunks;                 /**< Total chunked skills */
    float mean_chunk_size;               /**< Mean sub-skills per chunk */

    // Practice statistics
    uint64_t total_practice_sessions;    /**< Total practice sessions */
    float total_practice_time;           /**< Total time spent practicing */

} procedural_stats_t;

/**
 * @brief Result from skill execution
 *
 * WHAT: Information about execution outcome
 * WHY:  Return execution status and metrics
 */
typedef struct {
    uint64_t skill_id;                   /**< Executed skill ID */
    proc_exec_status_t status;           /**< Final status */
    size_t steps_completed;              /**< Steps successfully completed */
    size_t steps_failed;                 /**< Steps that failed */
    float execution_time_ms;             /**< Total execution time */
    float accuracy;                       /**< Execution accuracy */
    bool triggered_learning;             /**< Whether practice updated learning */
} procedural_exec_result_t;

/**
 * @brief Result from habit trigger check
 *
 * WHAT: Information about triggered habit
 * WHY:  Return triggered habit details for execution
 */
typedef struct {
    uint64_t habit_id;                   /**< Triggered habit ID */
    float trigger_strength;              /**< Strength of trigger match */
    float habit_strength;                /**< Current habit strength */
    float predicted_reward;              /**< Expected reward from execution */
    const prime_signature_t* response;   /**< Response signature */
} procedural_habit_result_t;

/**
 * @brief Procedural memory system manager (opaque)
 *
 * Internal structure containing:
 * - Skill storage array
 * - Habit storage array
 * - PR memory integration handles
 * - Execution state
 * - Thread synchronization primitives
 */
typedef struct procedural_memory_internal* procedural_memory_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default procedural memory configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Starting point for most use cases
 *
 * @return Default configuration
 *
 * Defaults:
 * - max_skills: 1024
 * - max_habits: 512
 * - max_steps_per_skill: 64
 * - base_learning_rate: 0.1
 * - skill_decay_rate: 0.05/day
 * - habit_trigger_threshold: 0.7
 */
NIMCP_EXPORT procedural_config_t procedural_config_default(void);

/**
 * @brief Validate procedural configuration
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 */
NIMCP_EXPORT bool procedural_config_validate(const procedural_config_t* config);

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create procedural memory system
 *
 * WHAT: Allocates and initializes procedural memory manager
 * WHY:  Entry point for procedural memory functionality
 * HOW:  Creates storage, initializes execution state, links to PR system
 *
 * @param entanglement Entanglement graph for memory associations (can be NULL)
 * @param node_manager PR memory node manager (can be NULL)
 * @param config Configuration (NULL for defaults)
 * @return Procedural memory handle or NULL on failure
 *
 * Memory: ~500KB for default configuration
 * Thread safety: Returned handle is thread-safe
 *
 * EXAMPLE:
 * ```c
 * procedural_memory_t pm = procedural_create(
 *     entangle_graph, node_mgr, NULL);
 * if (!pm) {
 *     fprintf(stderr, "Failed: %s\n", procedural_get_last_error());
 * }
 * ```
 */
NIMCP_EXPORT procedural_memory_t procedural_create(
    entangle_graph_t entanglement,
    pr_node_manager_t node_manager,
    const procedural_config_t* config
);

/**
 * @brief Destroy procedural memory system
 *
 * WHAT: Releases all resources
 * WHY:  Clean shutdown
 * HOW:  Frees skills, habits, execution state, handles
 *
 * @param pm Procedural memory to destroy (NULL safe)
 */
NIMCP_EXPORT void procedural_destroy(procedural_memory_t pm);

/**
 * @brief Reset procedural memory to initial state
 *
 * WHAT: Clears all skills and habits, resets statistics
 * WHY:  Fresh start without reallocation
 *
 * @param pm Procedural memory
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_reset(procedural_memory_t pm);

//=============================================================================
// Skill Creation Functions
//=============================================================================

/**
 * @brief Create a new skill
 *
 * WHAT: Creates empty skill shell to add steps to
 * WHY:  Build skills incrementally
 * HOW:  Allocates skill, initializes state, ready for steps
 *
 * @param pm Procedural memory
 * @param name Skill name (copied)
 * @param type Skill type (motor/cognitive/perceptual)
 * @param skill_id_out Output: created skill ID
 * @return PROC_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * uint64_t id;
 * procedural_create_skill(pm, "Touch Typing", PROC_MOTOR, &id);
 * ```
 */
NIMCP_EXPORT procedural_error_t procedural_create_skill(
    procedural_memory_t pm,
    const char* name,
    procedural_type_t type,
    uint64_t* skill_id_out
);

/**
 * @brief Create skill with initial steps
 *
 * WHAT: Creates skill with pre-defined action sequence
 * WHY:  Efficient creation when steps are known
 *
 * @param pm Procedural memory
 * @param name Skill name
 * @param type Skill type
 * @param step_descriptions Array of step descriptions
 * @param step_durations Array of expected durations (ms)
 * @param num_steps Number of steps
 * @param skill_id_out Output: created skill ID
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_create_skill_with_steps(
    procedural_memory_t pm,
    const char* name,
    procedural_type_t type,
    const char** step_descriptions,
    const float* step_durations,
    size_t num_steps,
    uint64_t* skill_id_out
);

/**
 * @brief Add step to existing skill
 *
 * WHAT: Appends action step to skill sequence
 * WHY:  Build skill incrementally
 * HOW:  Allocates step, adds to skill's step array
 *
 * @param pm Procedural memory
 * @param skill_id Skill to add step to
 * @param action_description Human-readable action
 * @param duration_ms Expected duration
 * @param precision_required Required precision [0, 1]
 * @param step_id_out Output: created step ID
 * @return PROC_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * uint64_t step_id;
 * procedural_add_step(pm, skill_id,
 *     "Place fingers on home row", 500.0f, 0.9f, &step_id);
 * ```
 */
NIMCP_EXPORT procedural_error_t procedural_add_step(
    procedural_memory_t pm,
    uint64_t skill_id,
    const char* action_description,
    float duration_ms,
    float precision_required,
    uint64_t* step_id_out
);

/**
 * @brief Add step with preconditions and postconditions
 *
 * WHAT: Adds step with explicit state requirements
 * WHY:  Model action dependencies
 *
 * @param pm Procedural memory
 * @param skill_id Skill to add step to
 * @param action_description Human-readable action
 * @param duration_ms Expected duration
 * @param precision_required Required precision
 * @param preconditions Required state before step (can be NULL)
 * @param postconditions Expected state after step (can be NULL)
 * @param step_id_out Output: created step ID
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_add_step_ex(
    procedural_memory_t pm,
    uint64_t skill_id,
    const char* action_description,
    float duration_ms,
    float precision_required,
    const prime_signature_t* preconditions,
    const prime_signature_t* postconditions,
    uint64_t* step_id_out
);

/**
 * @brief Insert step at specific position
 *
 * WHAT: Inserts step at given index in sequence
 * WHY:  Modify existing skill structure
 *
 * @param pm Procedural memory
 * @param skill_id Skill to modify
 * @param index Position to insert (0 = beginning)
 * @param action_description Human-readable action
 * @param duration_ms Expected duration
 * @param precision_required Required precision
 * @param step_id_out Output: created step ID
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_insert_step(
    procedural_memory_t pm,
    uint64_t skill_id,
    size_t index,
    const char* action_description,
    float duration_ms,
    float precision_required,
    uint64_t* step_id_out
);

/**
 * @brief Remove step from skill
 *
 * WHAT: Deletes step from skill sequence
 * WHY:  Modify skill structure
 *
 * @param pm Procedural memory
 * @param skill_id Skill to modify
 * @param step_index Index of step to remove
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_remove_step(
    procedural_memory_t pm,
    uint64_t skill_id,
    size_t step_index
);

//=============================================================================
// Practice and Learning Functions
//=============================================================================

/**
 * @brief Practice skill (simulate execution)
 *
 * WHAT: Performs practice session updating learning state
 * WHY:  Skills improve through practice
 * HOW:  Updates accuracy, automaticity, stage; may trigger stage transition
 *
 * @param pm Procedural memory
 * @param skill_id Skill to practice
 * @param practice_accuracy Accuracy achieved in this practice [0, 1]
 * @param practice_duration Duration of practice (ms)
 * @return PROC_SUCCESS or error code
 *
 * Effects:
 * - Increments practice_count
 * - Updates accuracy (running average)
 * - Updates automaticity (increases with practice)
 * - May trigger stage advancement
 * - Updates last_practice_time
 * - Strengthens skill
 *
 * EXAMPLE:
 * ```c
 * // User practiced typing, achieved 85% accuracy
 * procedural_practice(pm, typing_skill_id, 0.85f, 30000.0f);
 * ```
 */
NIMCP_EXPORT procedural_error_t procedural_practice(
    procedural_memory_t pm,
    uint64_t skill_id,
    float practice_accuracy,
    float practice_duration
);

/**
 * @brief Practice with step-by-step results
 *
 * WHAT: Detailed practice with per-step feedback
 * WHY:  Fine-grained learning updates
 *
 * @param pm Procedural memory
 * @param skill_id Skill to practice
 * @param step_accuracies Array of accuracy per step
 * @param step_durations Array of actual duration per step
 * @param num_steps Number of steps (must match skill)
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_practice_detailed(
    procedural_memory_t pm,
    uint64_t skill_id,
    const float* step_accuracies,
    const float* step_durations,
    size_t num_steps
);

/**
 * @brief Check and advance skill stage if ready
 *
 * WHAT: Evaluates if skill should advance to next stage
 * WHY:  Automatic stage progression based on learning
 * HOW:  Checks practice count, accuracy, automaticity thresholds
 *
 * @param pm Procedural memory
 * @param skill_id Skill to check
 * @param advanced_out Output: true if stage advanced
 * @return PROC_SUCCESS or error code
 *
 * Stage progression criteria:
 * - COGNITIVE -> ASSOCIATIVE:
 *   - practice_count >= 50
 *   - accuracy >= 0.7
 *   - automaticity >= 0.3
 *
 * - ASSOCIATIVE -> AUTONOMOUS:
 *   - practice_count >= 500
 *   - accuracy >= 0.9
 *   - automaticity >= 0.7
 */
NIMCP_EXPORT procedural_error_t procedural_advance_stage(
    procedural_memory_t pm,
    uint64_t skill_id,
    bool* advanced_out
);

/**
 * @brief Force set skill stage
 *
 * WHAT: Manually sets skill stage
 * WHY:  Testing or manual override
 *
 * @param pm Procedural memory
 * @param skill_id Skill to modify
 * @param stage New stage
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_set_stage(
    procedural_memory_t pm,
    uint64_t skill_id,
    skill_stage_t stage
);

/**
 * @brief Get current skill stage
 *
 * WHAT: Retrieves current acquisition stage
 * WHY:  Query skill learning progress
 *
 * @param pm Procedural memory
 * @param skill_id Skill to query
 * @return Current stage or -1 on error
 */
NIMCP_EXPORT skill_stage_t procedural_get_stage(
    procedural_memory_t pm,
    uint64_t skill_id
);

/**
 * @brief Compute skill automaticity
 *
 * WHAT: Calculates how automatic skill execution is
 * WHY:  Measure learning progress
 * HOW:  Based on practice count, accuracy stability, speed
 *
 * @param pm Procedural memory
 * @param skill_id Skill to evaluate
 * @return Automaticity [0, 1] or -1 on error
 *
 * Automaticity factors:
 * - Practice repetitions (logarithmic)
 * - Accuracy consistency
 * - Speed improvement
 * - Low error variability
 */
NIMCP_EXPORT float procedural_compute_automaticity(
    procedural_memory_t pm,
    uint64_t skill_id
);

//=============================================================================
// Execution Functions
//=============================================================================

/**
 * @brief Execute a skill
 *
 * WHAT: Runs skill execution, returns result
 * WHY:  Actually perform the learned skill
 * HOW:  Steps through action sequence, tracks performance
 *
 * @param pm Procedural memory
 * @param skill_id Skill to execute
 * @param result_out Output: execution result
 * @return PROC_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * procedural_exec_result_t result;
 * if (procedural_execute(pm, skill_id, &result) == PROC_SUCCESS) {
 *     printf("Executed with %.2f%% accuracy\n", result.accuracy * 100);
 * }
 * ```
 */
NIMCP_EXPORT procedural_error_t procedural_execute(
    procedural_memory_t pm,
    uint64_t skill_id,
    procedural_exec_result_t* result_out
);

/**
 * @brief Begin skill execution (async)
 *
 * WHAT: Starts skill execution, must call step/complete
 * WHY:  Incremental execution for real-time systems
 *
 * @param pm Procedural memory
 * @param skill_id Skill to execute
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_begin_execution(
    procedural_memory_t pm,
    uint64_t skill_id
);

/**
 * @brief Execute next step (async)
 *
 * WHAT: Executes current step and advances
 * WHY:  Step-by-step execution
 *
 * @param pm Procedural memory
 * @param step_success Whether step succeeded
 * @param step_duration Actual step duration
 * @param completed_out Output: true if skill completed
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_step_execution(
    procedural_memory_t pm,
    bool step_success,
    float step_duration,
    bool* completed_out
);

/**
 * @brief Complete/abort execution (async)
 *
 * WHAT: Finishes or aborts current execution
 * WHY:  Clean up execution state
 *
 * @param pm Procedural memory
 * @param success Whether execution succeeded overall
 * @param result_out Output: final result
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_end_execution(
    procedural_memory_t pm,
    bool success,
    procedural_exec_result_t* result_out
);

/**
 * @brief Get currently executing skill
 *
 * WHAT: Returns ID of skill being executed
 * WHY:  Query execution state
 *
 * @param pm Procedural memory
 * @return Skill ID or PROC_INVALID_ID if not executing
 */
NIMCP_EXPORT uint64_t procedural_get_executing_skill(procedural_memory_t pm);

/**
 * @brief Get current execution step index
 *
 * WHAT: Returns index of current step in execution
 * WHY:  Track execution progress
 *
 * @param pm Procedural memory
 * @return Step index or (size_t)-1 if not executing
 */
NIMCP_EXPORT size_t procedural_get_current_step(procedural_memory_t pm);

//=============================================================================
// Habit Functions
//=============================================================================

/**
 * @brief Create a stimulus-response habit
 *
 * WHAT: Creates automatic cue-response association
 * WHY:  Model habitual behaviors
 * HOW:  Associates cue signature with response signature
 *
 * @param pm Procedural memory
 * @param cue_signature Trigger cue signature
 * @param response_signature Response action signature
 * @param initial_strength Initial habit strength [0, 1] (0 for default)
 * @param habit_id_out Output: created habit ID
 * @return PROC_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * // Create habit: "see coffee machine -> make coffee"
 * prime_signature_t* cue = prime_sig_from_text("coffee machine");
 * prime_signature_t* response = prime_sig_from_text("make coffee");
 * uint64_t id;
 * procedural_create_habit(pm, cue, response, 0.0f, &id);
 * ```
 */
NIMCP_EXPORT procedural_error_t procedural_create_habit(
    procedural_memory_t pm,
    const prime_signature_t* cue_signature,
    const prime_signature_t* response_signature,
    float initial_strength,
    uint64_t* habit_id_out
);

/**
 * @brief Create habit linked to a skill
 *
 * WHAT: Creates habit that triggers skill execution
 * WHY:  Connect automatic triggers to learned skills
 *
 * @param pm Procedural memory
 * @param cue_signature Trigger cue signature
 * @param linked_skill_id Skill to execute when triggered
 * @param initial_strength Initial habit strength
 * @param habit_id_out Output: created habit ID
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_create_habit_for_skill(
    procedural_memory_t pm,
    const prime_signature_t* cue_signature,
    uint64_t linked_skill_id,
    float initial_strength,
    uint64_t* habit_id_out
);

/**
 * @brief Check if context triggers any habits
 *
 * WHAT: Matches context against all habit cues
 * WHY:  Detect when habits should fire
 * HOW:  Compares context signature to cue signatures
 *
 * @param pm Procedural memory
 * @param context_signature Current context signature
 * @param triggered_out Output array for triggered habits
 * @param max_triggered Maximum results to return
 * @param num_triggered_out Output: number triggered
 * @return PROC_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * procedural_habit_result_t triggered[16];
 * size_t num;
 * procedural_trigger_habit(pm, current_context, triggered, 16, &num);
 * for (size_t i = 0; i < num; i++) {
 *     printf("Habit %lu triggered with strength %.2f\n",
 *            triggered[i].habit_id, triggered[i].habit_strength);
 * }
 * ```
 */
NIMCP_EXPORT procedural_error_t procedural_trigger_habit(
    procedural_memory_t pm,
    const prime_signature_t* context_signature,
    procedural_habit_result_t* triggered_out,
    size_t max_triggered,
    size_t* num_triggered_out
);

/**
 * @brief Reinforce habit with reward
 *
 * WHAT: Updates habit strength based on reward
 * WHY:  Habits strengthen/weaken based on outcomes
 * HOW:  Dopaminergic reward prediction error learning
 *
 * @param pm Procedural memory
 * @param habit_id Habit to reinforce
 * @param reward Reward value (positive strengthens, negative weakens)
 * @return New habit strength or -1 on error
 *
 * Learning rule (simplified TD):
 * delta = learning_rate * (reward - prediction)
 * strength += delta
 * prediction += learning_rate * (reward - prediction)
 *
 * EXAMPLE:
 * ```c
 * // User got coffee, reinforce the habit
 * procedural_reinforce_habit(pm, coffee_habit_id, 1.0f);
 * ```
 */
NIMCP_EXPORT float procedural_reinforce_habit(
    procedural_memory_t pm,
    uint64_t habit_id,
    float reward
);

/**
 * @brief Get habit by ID
 *
 * WHAT: Retrieves habit details
 * WHY:  Query habit state
 *
 * @param pm Procedural memory
 * @param habit_id Habit to retrieve
 * @param habit_out Output: habit copy
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_get_habit(
    procedural_memory_t pm,
    uint64_t habit_id,
    procedural_habit_t* habit_out
);

/**
 * @brief Remove habit
 *
 * WHAT: Deletes habit from system
 * WHY:  Break unwanted habits
 *
 * @param pm Procedural memory
 * @param habit_id Habit to remove
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_remove_habit(
    procedural_memory_t pm,
    uint64_t habit_id
);

//=============================================================================
// Chunking Functions
//=============================================================================

/**
 * @brief Combine sub-skills into a chunk
 *
 * WHAT: Creates composite skill from multiple sub-skills
 * WHY:  Chunking reduces cognitive load
 * HOW:  Links sub-skills into single execution unit
 *
 * @param pm Procedural memory
 * @param name Chunk name
 * @param sub_skill_ids Array of sub-skill IDs
 * @param num_sub_skills Number of sub-skills
 * @param chunk_id_out Output: created chunk ID
 * @return PROC_SUCCESS or error code
 *
 * EXAMPLE:
 * ```c
 * // Combine letter typing into word typing
 * uint64_t letter_skills[] = {t_skill, h_skill, e_skill};
 * uint64_t chunk_id;
 * procedural_chunk_skills(pm, "Type 'the'", letter_skills, 3, &chunk_id);
 * ```
 */
NIMCP_EXPORT procedural_error_t procedural_chunk_skills(
    procedural_memory_t pm,
    const char* name,
    const uint64_t* sub_skill_ids,
    size_t num_sub_skills,
    uint64_t* chunk_id_out
);

/**
 * @brief Get sub-skills of a chunk
 *
 * WHAT: Retrieves component sub-skills
 * WHY:  Inspect chunk composition
 *
 * @param pm Procedural memory
 * @param chunk_id Chunk to query
 * @param sub_skill_ids_out Output array for sub-skill IDs
 * @param max_sub_skills Maximum to return
 * @param num_sub_skills_out Output: actual count
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_get_sub_skills(
    procedural_memory_t pm,
    uint64_t chunk_id,
    uint64_t* sub_skill_ids_out,
    size_t max_sub_skills,
    size_t* num_sub_skills_out
);

/**
 * @brief Check if skill is a chunk
 *
 * @param pm Procedural memory
 * @param skill_id Skill to check
 * @return true if chunk, false otherwise
 */
NIMCP_EXPORT bool procedural_is_chunk(
    procedural_memory_t pm,
    uint64_t skill_id
);

//=============================================================================
// Decay Functions
//=============================================================================

/**
 * @brief Apply decay to unpracticed skills
 *
 * WHAT: Reduces strength of skills not recently practiced
 * WHY:  Model forgetting of unused skills
 * HOW:  Exponential decay based on time since last practice
 *
 * @param pm Procedural memory
 * @param elapsed_days Days since last decay application
 * @return Number of skills affected
 *
 * Decay formula: strength *= exp(-decay_rate * elapsed_days)
 */
NIMCP_EXPORT size_t procedural_decay(
    procedural_memory_t pm,
    float elapsed_days
);

/**
 * @brief Apply decay to specific skill
 *
 * @param pm Procedural memory
 * @param skill_id Skill to decay
 * @param elapsed_days Days since last practice
 * @return New strength or -1 on error
 */
NIMCP_EXPORT float procedural_decay_skill(
    procedural_memory_t pm,
    uint64_t skill_id,
    float elapsed_days
);

/**
 * @brief Apply decay to habits
 *
 * WHAT: Reduces strength of unused habits
 * WHY:  Habits weaken without reinforcement
 *
 * @param pm Procedural memory
 * @param elapsed_days Days since last decay
 * @return Number of habits affected
 */
NIMCP_EXPORT size_t procedural_decay_habits(
    procedural_memory_t pm,
    float elapsed_days
);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get skill by ID
 *
 * WHAT: Retrieves skill details
 * WHY:  Query skill state
 *
 * @param pm Procedural memory
 * @param skill_id Skill to retrieve
 * @param skill_out Output: skill copy (caller-allocated)
 * @return PROC_SUCCESS or error code
 *
 * Note: skill_name and steps point to internal storage
 */
NIMCP_EXPORT procedural_error_t procedural_get_skill(
    procedural_memory_t pm,
    uint64_t skill_id,
    procedural_skill_t* skill_out
);

/**
 * @brief Get all skills of a type
 *
 * @param pm Procedural memory
 * @param type Skill type to filter by
 * @param skill_ids_out Output array for skill IDs
 * @param max_skills Maximum to return
 * @param count_out Output: actual count
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_get_skills_by_type(
    procedural_memory_t pm,
    procedural_type_t type,
    uint64_t* skill_ids_out,
    size_t max_skills,
    size_t* count_out
);

/**
 * @brief Get all skills at a stage
 *
 * @param pm Procedural memory
 * @param stage Stage to filter by
 * @param skill_ids_out Output array
 * @param max_skills Maximum to return
 * @param count_out Output: actual count
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_get_skills_by_stage(
    procedural_memory_t pm,
    skill_stage_t stage,
    uint64_t* skill_ids_out,
    size_t max_skills,
    size_t* count_out
);

/**
 * @brief Search skills by name
 *
 * @param pm Procedural memory
 * @param query Search string (substring match)
 * @param skill_ids_out Output array
 * @param max_skills Maximum to return
 * @param count_out Output: actual count
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_search_skills(
    procedural_memory_t pm,
    const char* query,
    uint64_t* skill_ids_out,
    size_t max_skills,
    size_t* count_out
);

/**
 * @brief Get strongest skills (by strength)
 *
 * @param pm Procedural memory
 * @param k Number of skills to return
 * @param skill_ids_out Output array
 * @param count_out Output: actual count
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_get_strongest_skills(
    procedural_memory_t pm,
    size_t k,
    uint64_t* skill_ids_out,
    size_t* count_out
);

/**
 * @brief Remove skill
 *
 * WHAT: Deletes skill from system
 * WHY:  Clean up unused skills
 *
 * @param pm Procedural memory
 * @param skill_id Skill to remove
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_remove_skill(
    procedural_memory_t pm,
    uint64_t skill_id
);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * @brief Get procedural memory statistics
 *
 * @param pm Procedural memory
 * @param stats_out Output statistics structure
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_get_stats(
    procedural_memory_t pm,
    procedural_stats_t* stats_out
);

/**
 * @brief Reset statistics counters
 *
 * @param pm Procedural memory
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_reset_stats(procedural_memory_t pm);

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* procedural_error_string(procedural_error_t error);

/**
 * @brief Get last error message
 *
 * @return Error string from last failed operation
 */
NIMCP_EXPORT const char* procedural_get_last_error(void);

/**
 * @brief Get skill stage name
 *
 * @param stage Skill stage
 * @return Human-readable stage name
 */
NIMCP_EXPORT const char* procedural_stage_name(skill_stage_t stage);

/**
 * @brief Get procedural type name
 *
 * @param type Procedural type
 * @return Human-readable type name
 */
NIMCP_EXPORT const char* procedural_type_name(procedural_type_t type);

/**
 * @brief Get execution status name
 *
 * @param status Execution status
 * @return Human-readable status name
 */
NIMCP_EXPORT const char* procedural_exec_status_name(proc_exec_status_t status);

/**
 * @brief Print skill details (debug)
 *
 * @param skill Skill to print
 */
NIMCP_EXPORT void procedural_print_skill(const procedural_skill_t* skill);

/**
 * @brief Print habit details (debug)
 *
 * @param habit Habit to print
 */
NIMCP_EXPORT void procedural_print_habit(const procedural_habit_t* habit);

/**
 * @brief Print system summary (debug)
 *
 * @param pm Procedural memory
 */
NIMCP_EXPORT void procedural_print_summary(procedural_memory_t pm);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get current time (utility)
 *
 * @return Current time in seconds
 */
NIMCP_EXPORT float procedural_current_time(void);

/**
 * @brief Compute learning rate based on stage and history
 *
 * @param pm Procedural memory
 * @param skill_id Skill to compute for
 * @return Learning rate [0, 1] or -1 on error
 *
 * Learning rate decreases:
 * - With higher stage (autonomous learns slower)
 * - With more practice (diminishing returns)
 * - With higher accuracy (less room to improve)
 */
NIMCP_EXPORT float procedural_compute_learning_rate(
    procedural_memory_t pm,
    uint64_t skill_id
);

/**
 * @brief Estimate time to master skill (reach autonomous)
 *
 * @param pm Procedural memory
 * @param skill_id Skill to estimate for
 * @return Estimated time in days or -1 on error
 */
NIMCP_EXPORT float procedural_estimate_mastery_time(
    procedural_memory_t pm,
    uint64_t skill_id
);

//=============================================================================
// Integration Functions
//=============================================================================

/**
 * @brief Get underlying PR memory node for skill
 *
 * @param pm Procedural memory
 * @param skill_id Skill to query
 * @return PR memory node or NULL
 */
NIMCP_EXPORT pr_memory_node_t* procedural_get_skill_node(
    procedural_memory_t pm,
    uint64_t skill_id
);

/**
 * @brief Get underlying PR memory node for habit
 *
 * @param pm Procedural memory
 * @param habit_id Habit to query
 * @return PR memory node or NULL
 */
NIMCP_EXPORT pr_memory_node_t* procedural_get_habit_node(
    procedural_memory_t pm,
    uint64_t habit_id
);

/**
 * @brief Update skill signature based on current steps
 *
 * @param pm Procedural memory
 * @param skill_id Skill to update
 * @return PROC_SUCCESS or error code
 */
NIMCP_EXPORT procedural_error_t procedural_update_skill_signature(
    procedural_memory_t pm,
    uint64_t skill_id
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PROCEDURAL_H
