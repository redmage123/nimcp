//=============================================================================
// nimcp_bg_sequence_chunking.h - Action Sequence Chunking System
//=============================================================================
/**
 * @file nimcp_bg_sequence_chunking.h
 * @brief Action sequence learning and chunking for skill acquisition
 *
 * WHAT: Models how basal ganglia learns and executes action sequences as chunks
 * WHY:  Enables skill learning where multi-step behaviors become single units
 * HOW:  Sequences are learned, chunked, and can be executed as automatized units
 *
 * BIOLOGICAL BASIS:
 * - Basal ganglia is essential for learning action sequences
 * - Repeated sequences become "chunked" - executed as single units
 * - Chunks have:
 *   - Initiation: Context/cue that triggers the chunk
 *   - Execution: Automatic sequence progression
 *   - Termination: Condition that ends the chunk
 * - Chunking involves DLS (dorsolateral striatum) for habits
 * - Early learning involves DMS (dorsomedial striatum) for goal-directed
 *
 * BIDIRECTIONAL DATA FLOW:
 * - Cortex → Chunk: Provides initiation cues and context
 * - Chunk → Cortex: Provides current action, progress feedback
 * - Chunk → Striatum: Drives action selection during execution
 * - Striatum → Chunk: Provides action completion signals
 * - Chunk → Dopamine: Requests reward prediction for transitions
 * - Dopamine → Chunk: Modulates learning and transition strength
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_BG_SEQUENCE_CHUNKING_H
#define NIMCP_BG_SEQUENCE_CHUNKING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BGSC_MAX_CHUNKS 64              /**< Maximum number of chunks */
#define BGSC_MAX_SEQUENCE_LENGTH 32     /**< Maximum actions in a sequence */
#define BGSC_MIN_REPETITIONS 5          /**< Minimum reps to form chunk */
#define BGSC_AUTOMATICITY_THRESHOLD 0.8f /**< Threshold for automatic execution */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Chunk learning stage
 */
typedef enum {
    BGSC_STAGE_NAIVE = 0,               /**< Not yet learned */
    BGSC_STAGE_LEARNING,                /**< Actively being learned */
    BGSC_STAGE_CONSOLIDATING,           /**< Consolidating (sleep-dependent) */
    BGSC_STAGE_CHUNKED,                 /**< Fully chunked/automatized */
    BGSC_STAGE_DEGRADED                 /**< Degraded from disuse */
} bgsc_stage_t;

/**
 * @brief Chunk execution state
 */
typedef enum {
    BGSC_EXEC_IDLE = 0,                 /**< Not executing */
    BGSC_EXEC_INITIATED,                /**< Chunk initiated, first action */
    BGSC_EXEC_RUNNING,                  /**< Mid-sequence execution */
    BGSC_EXEC_PAUSED,                   /**< Temporarily paused */
    BGSC_EXEC_COMPLETING,               /**< Final action */
    BGSC_EXEC_TERMINATED,               /**< Terminated (normal or early) */
    BGSC_EXEC_ABORTED                   /**< Aborted due to error */
} bgsc_exec_state_t;

/**
 * @brief Termination condition
 */
typedef enum {
    BGSC_TERM_SEQUENCE_COMPLETE = 0,    /**< All actions completed */
    BGSC_TERM_GOAL_ACHIEVED,            /**< Goal state reached */
    BGSC_TERM_TIMEOUT,                  /**< Timeout occurred */
    BGSC_TERM_EXTERNAL_STOP,            /**< External stop signal */
    BGSC_TERM_ERROR                     /**< Error during execution */
} bgsc_termination_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Single action in a sequence
 */
typedef struct {
    uint32_t action_id;                 /**< Action identifier */
    float expected_duration_ms;         /**< Expected duration */
    float transition_strength;          /**< Strength of transition to next */
    uint32_t times_executed;            /**< Execution count */
    float avg_duration_ms;              /**< Average actual duration */
} bgsc_sequence_action_t;

/**
 * @brief Action chunk
 */
typedef struct {
    uint32_t chunk_id;                  /**< Chunk identifier */
    char name[64];                      /**< Chunk name/description */

    /* Sequence */
    bgsc_sequence_action_t* actions;    /**< Ordered action sequence */
    uint32_t sequence_length;           /**< Number of actions */
    uint32_t max_length;                /**< Maximum sequence length */

    /* Learning state */
    bgsc_stage_t stage;                 /**< Learning stage */
    float automaticity;                 /**< Automaticity level [0-1] */
    uint32_t total_executions;          /**< Total execution count */
    uint32_t successful_executions;     /**< Successful completions */
    float success_rate;                 /**< Success rate */

    /* Initiation */
    uint32_t trigger_context;           /**< Context that triggers chunk */
    float initiation_threshold;         /**< Threshold to initiate */
    float current_initiation;           /**< Current initiation activation */

    /* Termination */
    bgsc_termination_t last_termination; /**< How last execution ended */
    float termination_threshold;        /**< Threshold for early termination */

    /* Execution state */
    bgsc_exec_state_t exec_state;       /**< Current execution state */
    uint32_t current_step;              /**< Current step in sequence */
    uint64_t exec_start_time_ms;        /**< Execution start time */
    uint64_t step_start_time_ms;        /**< Current step start time */

    /* Feedback (bidirectional) */
    float cortical_feedback;            /**< Feedback to cortex */
    float progress;                     /**< Execution progress [0-1] */
    float reward_prediction;            /**< Expected reward at completion */
} bgsc_chunk_t;

/**
 * @brief Chunk system configuration
 */
typedef struct {
    uint32_t max_chunks;                /**< Maximum chunks */
    uint32_t max_sequence_length;       /**< Maximum sequence length */
    float learning_rate;                /**< Chunk learning rate */
    float decay_rate;                   /**< Unused chunk decay rate */
    float automaticity_threshold;       /**< Threshold for auto-execution */
    uint32_t min_reps_to_chunk;         /**< Minimum reps to form chunk */
    bool enable_chunking;               /**< Enable automatic chunking */
    bool enable_early_termination;      /**< Allow early termination */
} bgsc_config_t;

/**
 * @brief Chunk system statistics
 */
typedef struct {
    uint32_t total_chunks;              /**< Total chunks registered */
    uint32_t active_chunks;             /**< Currently active chunks */
    uint32_t automatized_chunks;        /**< Fully automatized chunks */
    uint64_t total_executions;          /**< Total chunk executions */
    uint64_t successful_executions;     /**< Successful completions */
    float avg_sequence_length;          /**< Average sequence length */
    float avg_automaticity;             /**< Average automaticity level */
} bgsc_stats_t;

/**
 * @brief Bidirectional data packet for chunk system
 */
typedef struct {
    /* Input (from other systems) */
    float cortical_input;               /**< Input from cortex */
    float dopamine_level;               /**< Current dopamine level */
    bool action_completed;              /**< Action completion signal */
    uint32_t completed_action_id;       /**< Which action completed */
    bool external_stop;                 /**< External stop signal */

    /* Output (to other systems) */
    uint32_t requested_action;          /**< Action to execute */
    float action_urgency;               /**< Urgency of action */
    float progress_feedback;            /**< Progress to cortex */
    float reward_prediction;            /**< Expected reward */
    bool chunk_active;                  /**< Whether chunk is active */
} bgsc_bidir_data_t;

/**
 * @brief Chunk system
 */
typedef struct bgsc_system bgsc_system_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration
 */
void bgsc_default_config(bgsc_config_t* config);

/**
 * @brief Create chunk system
 */
bgsc_system_t* bgsc_create(const bgsc_config_t* config);

/**
 * @brief Destroy chunk system
 */
void bgsc_destroy(bgsc_system_t* system);

/**
 * @brief Reset chunk system
 */
int bgsc_reset(bgsc_system_t* system);

//=============================================================================
// Chunk Registration Functions
//=============================================================================

/**
 * @brief Register a new chunk
 * @param system Chunk system
 * @param name Chunk name
 * @param trigger_context Context that triggers the chunk
 * @param chunk_id Output: new chunk ID
 * @return 0 on success
 */
int bgsc_register_chunk(bgsc_system_t* system,
                         const char* name,
                         uint32_t trigger_context,
                         uint32_t* chunk_id);

/**
 * @brief Add action to chunk sequence
 * @param system Chunk system
 * @param chunk_id Chunk to modify
 * @param action_id Action to add
 * @param expected_duration_ms Expected duration
 * @return 0 on success
 */
int bgsc_add_action(bgsc_system_t* system,
                     uint32_t chunk_id,
                     uint32_t action_id,
                     float expected_duration_ms);

/**
 * @brief Set chunk initiation threshold
 */
int bgsc_set_initiation_threshold(bgsc_system_t* system,
                                   uint32_t chunk_id,
                                   float threshold);

/**
 * @brief Unregister chunk
 */
int bgsc_unregister_chunk(bgsc_system_t* system, uint32_t chunk_id);

//=============================================================================
// Execution Functions
//=============================================================================

/**
 * @brief Check if context triggers any chunk
 * @param system Chunk system
 * @param context Current context
 * @param chunk_id Output: triggered chunk (if any)
 * @return true if chunk triggered
 */
bool bgsc_check_trigger(bgsc_system_t* system,
                         uint32_t context,
                         uint32_t* chunk_id);

/**
 * @brief Initiate chunk execution
 * @param system Chunk system
 * @param chunk_id Chunk to execute
 * @return 0 on success
 */
int bgsc_initiate(bgsc_system_t* system, uint32_t chunk_id);

/**
 * @brief Get current action from executing chunk
 * @param system Chunk system
 * @param action_id Output: current action
 * @param urgency Output: action urgency
 * @return 0 on success, -1 if no chunk executing
 */
int bgsc_get_current_action(bgsc_system_t* system,
                             uint32_t* action_id,
                             float* urgency);

/**
 * @brief Signal action completion (bidirectional feedback)
 * @param system Chunk system
 * @param action_id Completed action
 * @param success Whether action succeeded
 * @param duration_ms Actual duration
 * @return 0 on success
 */
int bgsc_action_completed(bgsc_system_t* system,
                           uint32_t action_id,
                           bool success,
                           float duration_ms);

/**
 * @brief Pause chunk execution
 */
int bgsc_pause(bgsc_system_t* system);

/**
 * @brief Resume chunk execution
 */
int bgsc_resume(bgsc_system_t* system);

/**
 * @brief Abort chunk execution
 */
int bgsc_abort(bgsc_system_t* system);

/**
 * @brief Check if chunk is currently executing
 */
bool bgsc_is_executing(const bgsc_system_t* system);

/**
 * @brief Get execution progress [0-1]
 */
float bgsc_get_progress(const bgsc_system_t* system);

//=============================================================================
// Bidirectional Data Flow Functions
//=============================================================================

/**
 * @brief Process bidirectional data exchange
 *
 * This is the main interface for bidirectional data flow.
 * Input: cortical input, dopamine, action completion signals
 * Output: requested action, progress feedback, reward prediction
 *
 * @param system Chunk system
 * @param data Bidirectional data packet (input/output)
 * @return 0 on success
 */
int bgsc_process_bidir(bgsc_system_t* system, bgsc_bidir_data_t* data);

/**
 * @brief Set dopamine level (modulates learning)
 */
int bgsc_set_dopamine(bgsc_system_t* system, float dopamine);

/**
 * @brief Get feedback to cortex
 */
float bgsc_get_cortical_feedback(const bgsc_system_t* system);

/**
 * @brief Get reward prediction for current chunk
 */
float bgsc_get_reward_prediction(const bgsc_system_t* system);

//=============================================================================
// Learning Functions
//=============================================================================

/**
 * @brief Learn from executed sequence
 *
 * Called after a sequence of actions to potentially form a new chunk
 * or strengthen an existing one.
 *
 * @param system Chunk system
 * @param actions Executed action sequence
 * @param num_actions Number of actions
 * @param reward Final reward received
 * @return 0 on success
 */
int bgsc_learn_sequence(bgsc_system_t* system,
                         const uint32_t* actions,
                         uint32_t num_actions,
                         float reward);

/**
 * @brief Strengthen chunk from successful execution
 */
int bgsc_strengthen_chunk(bgsc_system_t* system,
                           uint32_t chunk_id,
                           float reward);

/**
 * @brief Weaken chunk from failed execution
 */
int bgsc_weaken_chunk(bgsc_system_t* system,
                       uint32_t chunk_id);

/**
 * @brief Get chunk automaticity level
 */
float bgsc_get_automaticity(const bgsc_system_t* system, uint32_t chunk_id);

/**
 * @brief Check if chunk is fully automatized
 */
bool bgsc_is_automatized(const bgsc_system_t* system, uint32_t chunk_id);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get chunk by ID
 */
const bgsc_chunk_t* bgsc_get_chunk(const bgsc_system_t* system, uint32_t chunk_id);

/**
 * @brief Get currently executing chunk ID
 * @return Chunk ID, or UINT32_MAX if none
 */
uint32_t bgsc_get_executing_chunk(const bgsc_system_t* system);

/**
 * @brief Get chunk execution state
 */
bgsc_exec_state_t bgsc_get_exec_state(const bgsc_system_t* system, uint32_t chunk_id);

/**
 * @brief Get system statistics
 */
int bgsc_get_stats(const bgsc_system_t* system, bgsc_stats_t* stats);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Step the chunk system
 * @param system Chunk system
 * @param dt_ms Time step in milliseconds
 * @return 0 on success
 */
int bgsc_step(bgsc_system_t* system, float dt_ms);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get stage name
 */
const char* bgsc_stage_name(bgsc_stage_t stage);

/**
 * @brief Get execution state name
 */
const char* bgsc_exec_state_name(bgsc_exec_state_t state);

/**
 * @brief Get termination name
 */
const char* bgsc_termination_name(bgsc_termination_t term);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BG_SEQUENCE_CHUNKING_H */
