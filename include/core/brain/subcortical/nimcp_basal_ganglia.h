//=============================================================================
// nimcp_basal_ganglia.h - Basal Ganglia Action Selection System
//=============================================================================
/**
 * @file nimcp_basal_ganglia.h
 * @brief Basal ganglia implementation for action selection and habit learning
 *
 * WHAT: Biologically-inspired basal ganglia model with direct/indirect/hyperdirect pathways
 * WHY:  Action selection, habit formation, and motor control
 * HOW:  Implements striatum (D1/D2), GPe/GPi, STN, and SNc/SNr with dopamine modulation
 *
 * BIOLOGICAL BASIS:
 * - The basal ganglia are a group of subcortical nuclei involved in action selection,
 *   motor control, habit learning, and reward processing
 * - Three main pathways:
 *   1. Direct pathway (GO): Striatum D1 → GPi/SNr → Thalamus (disinhibition)
 *   2. Indirect pathway (NO-GO): Striatum D2 → GPe → STN → GPi/SNr (increased inhibition)
 *   3. Hyperdirect pathway: Cortex → STN → GPi/SNr (fast global inhibition)
 * - Dopamine from SNc modulates D1 (excitatory) and D2 (inhibitory) medium spiny neurons
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#ifndef NIMCP_BASAL_GANGLIA_H
#define NIMCP_BASAL_GANGLIA_H

#include <stdint.h>
#include <stdbool.h>

#include "core/brain/subcortical/nimcp_striatum.h"
#include "core/brain/subcortical/nimcp_globus_pallidus.h"
#include "core/brain/subcortical/nimcp_substantia_nigra.h"
#include "core/brain/subcortical/nimcp_subthalamic.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define BG_MAX_ACTIONS 64                   /**< Maximum number of competing actions */
#define BG_MAX_HABITS 32                    /**< Maximum number of learned habits */
#define BG_DOPAMINE_BASELINE 0.5f           /**< Baseline dopamine level */
#define BG_ACTION_THRESHOLD 0.5f            /**< Threshold for action selection */
#define BG_HABIT_STRENGTH_THRESHOLD 0.7f    /**< Threshold for habit mode transition */

//=============================================================================
// Enums
//=============================================================================

/**
 * @brief Basal ganglia operating mode
 */
typedef enum {
    BG_MODE_GOAL_DIRECTED = 0,  /**< Goal-directed behavior (PFC involvement) */
    BG_MODE_HABITUAL,           /**< Habitual behavior (automatic) */
    BG_MODE_EXPLORATORY,        /**< Exploratory behavior (high uncertainty) */
    BG_MODE_SUPPRESSED          /**< Movement suppression (high STN activity) */
} bg_operating_mode_t;

/**
 * @brief Action selection state
 */
typedef enum {
    ACTION_STATE_IDLE = 0,      /**< No action being considered */
    ACTION_STATE_COMPETING,     /**< Multiple actions competing */
    ACTION_STATE_SELECTED,      /**< Action has been selected */
    ACTION_STATE_EXECUTING,     /**< Action is being executed */
    ACTION_STATE_COMPLETED,     /**< Action completed */
    ACTION_STATE_CANCELLED      /**< Action was cancelled */
} action_state_t;

/**
 * @brief Dopamine signal type
 */
typedef enum {
    DOPAMINE_SIGNAL_NONE = 0,   /**< No dopamine signal */
    DOPAMINE_SIGNAL_REWARD,     /**< Positive prediction error (phasic burst) */
    DOPAMINE_SIGNAL_PUNISHMENT, /**< Negative prediction error (phasic pause) */
    DOPAMINE_SIGNAL_TONIC       /**< Tonic baseline activity */
} dopamine_signal_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief Action candidate for selection
 */
typedef struct {
    uint32_t action_id;             /**< Unique action identifier */
    char name[64];                  /**< Action name/description */
    float value;                    /**< Expected value (Q-value) */
    float urgency;                  /**< Urgency/priority [0-1] */
    float cost;                     /**< Energy/effort cost [0-1] */
    float d1_activation;            /**< Direct pathway activation */
    float d2_activation;            /**< Indirect pathway activation */
    float net_activation;           /**< Net activation (d1 - d2) */
    bool is_habit;                  /**< True if this is a learned habit */
    float habit_strength;           /**< Strength of habit [0-1] */
} action_candidate_t;

/**
 * @brief Habit representation
 */
typedef struct {
    uint32_t habit_id;              /**< Unique habit identifier */
    uint32_t trigger_context;       /**< Context that triggers this habit */
    uint32_t action_id;             /**< Associated action */
    float strength;                 /**< Habit strength [0-1] */
    uint32_t repetitions;           /**< Number of times executed */
    uint64_t last_executed_ms;      /**< Last execution timestamp */
    float success_rate;             /**< Historical success rate */
} habit_t;

/**
 * @brief Basal ganglia configuration
 */
typedef struct {
    uint32_t num_actions;           /**< Number of possible actions */
    float dopamine_baseline;        /**< Baseline dopamine level */
    float action_threshold;         /**< Threshold for action selection */
    float habit_learning_rate;      /**< Rate of habit formation */
    float exploration_rate;         /**< Exploration vs exploitation */
    bool enable_hyperdirect;        /**< Enable hyperdirect pathway */
    bool enable_habit_learning;     /**< Enable habit formation */

    /* Component configurations */
    striatum_config_t striatum_config;
    globus_pallidus_config_t gpi_config;
    globus_pallidus_config_t gpe_config;
    subthalamic_config_t stn_config;
    substantia_nigra_config_t snc_config;
    substantia_nigra_config_t snr_config;
} basal_ganglia_config_t;

/**
 * @brief Basal ganglia statistics
 */
typedef struct {
    uint64_t total_selections;      /**< Total actions selected */
    uint64_t goal_directed_count;   /**< Goal-directed selections */
    uint64_t habitual_count;        /**< Habitual selections */
    uint64_t exploration_count;     /**< Exploratory selections */
    uint64_t suppression_count;     /**< Suppressed actions */
    float avg_selection_time_ms;    /**< Average selection latency */
    float avg_dopamine_level;       /**< Average dopamine level */
    float conflict_ratio;           /**< Ratio of high-conflict decisions */
} basal_ganglia_stats_t;

/**
 * @brief Main basal ganglia system
 */
typedef struct basal_ganglia {
    /* Component nuclei */
    striatum_t* striatum;                   /**< Striatum (input) */
    globus_pallidus_t* gpi;                 /**< Internal segment (output) */
    globus_pallidus_t* gpe;                 /**< External segment */
    subthalamic_nucleus_t* stn;             /**< Subthalamic nucleus */
    substantia_nigra_t* snc;                /**< Pars compacta (dopamine) */
    substantia_nigra_t* snr;                /**< Pars reticulata (output) */

    /* Action selection state */
    uint32_t num_actions;                   /**< Number of possible actions */
    action_candidate_t* actions;            /**< Action candidates */
    uint32_t selected_action;               /**< Currently selected action */
    action_state_t action_state;            /**< Current action state */

    /* Pathway activations */
    float* direct_pathway;                  /**< Direct pathway activations */
    float* indirect_pathway;                /**< Indirect pathway activations */
    float* hyperdirect_pathway;             /**< Hyperdirect pathway activations */
    float* thalamic_output;                 /**< Output to thalamus */

    /* Dopamine modulation */
    float dopamine_level;                   /**< Current dopamine level */
    dopamine_signal_t dopamine_signal;      /**< Current dopamine signal type */
    float reward_prediction_error;          /**< TD error for learning */

    /* Habit system */
    habit_t* habits;                        /**< Learned habits */
    uint32_t num_habits;                    /**< Current number of habits */
    uint32_t max_habits;                    /**< Maximum habits */
    bool habit_mode;                        /**< True = habitual, False = goal-directed */

    /* Operating mode */
    bg_operating_mode_t mode;               /**< Current operating mode */
    float conflict_level;                   /**< Level of action conflict [0-1] */
    float urgency_level;                    /**< Overall urgency [0-1] */

    /* Configuration */
    basal_ganglia_config_t config;          /**< Configuration */

    /* Statistics */
    basal_ganglia_stats_t stats;            /**< Runtime statistics */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;           /**< Bio-async context */
    bool bio_async_enabled;                 /**< Bio-async connected */

    /* MCTS integration */
    uint32_t rand_seed;                     /**< Thread-safe RNG seed for MCTS */
    float mcts_conflict_threshold;          /**< Threshold to trigger MCTS [0-1] */
    uint32_t mcts_iterations;               /**< MCTS iterations for uncertain decisions */

    /* Thread safety */
    nimcp_mutex_t* mutex;                   /**< Mutex for thread safety */
} basal_ganglia_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Initialize default configuration
 * @param config Configuration to initialize
 */
void basal_ganglia_default_config(basal_ganglia_config_t* config);

/**
 * @brief Create basal ganglia system
 * @param config Configuration (NULL for defaults)
 * @return New basal ganglia instance or NULL on failure
 */
basal_ganglia_t* basal_ganglia_create(const basal_ganglia_config_t* config);

/**
 * @brief Destroy basal ganglia system
 * @param bg System to destroy
 */
void basal_ganglia_destroy(basal_ganglia_t* bg);

/**
 * @brief Reset basal ganglia to initial state
 * @param bg System to reset
 * @return 0 on success, negative on error
 */
int basal_ganglia_reset(basal_ganglia_t* bg);

//=============================================================================
// Action Selection Functions
//=============================================================================

/**
 * @brief Set action values for selection
 * @param bg Basal ganglia system
 * @param action_id Action identifier
 * @param value Expected value (Q-value)
 * @param urgency Urgency level [0-1]
 * @param cost Action cost [0-1]
 * @return 0 on success, negative on error
 */
int basal_ganglia_set_action_value(
    basal_ganglia_t* bg,
    uint32_t action_id,
    float value,
    float urgency,
    float cost
);

/**
 * @brief Perform action selection
 *
 * Runs competition through direct/indirect/hyperdirect pathways
 *
 * @param bg Basal ganglia system
 * @param cortical_input Cortical input activations (size num_actions)
 * @param selected_action Output: selected action ID
 * @return 0 on success, negative on error
 */
int basal_ganglia_select_action(
    basal_ganglia_t* bg,
    const float* cortical_input,
    uint32_t* selected_action
);

/**
 * @brief Get thalamic output (motor command activation)
 * @param bg Basal ganglia system
 * @param output Output buffer (size num_actions)
 * @return 0 on success, negative on error
 */
int basal_ganglia_get_thalamic_output(
    const basal_ganglia_t* bg,
    float* output
);

/**
 * @brief Suppress action (emergency stop via hyperdirect pathway)
 * @param bg Basal ganglia system
 * @param suppression_strength Strength of suppression [0-1]
 * @return 0 on success, negative on error
 */
int basal_ganglia_suppress_action(
    basal_ganglia_t* bg,
    float suppression_strength
);

/**
 * @brief Mark action as completed
 * @param bg Basal ganglia system
 * @param action_id Completed action
 * @param success Whether action was successful
 * @return 0 on success, negative on error
 */
int basal_ganglia_action_completed(
    basal_ganglia_t* bg,
    uint32_t action_id,
    bool success
);

/**
 * @brief MCTS-based action selection for high-conflict decisions
 *
 * Uses Monte Carlo Tree Search to explore action consequences when
 * conflict between competing actions is high. Simulates multi-step
 * trajectories to find optimal action.
 *
 * @param bg Basal ganglia system
 * @param cortical_input Cortical input activations (size num_actions)
 * @param num_iterations MCTS iterations (0 uses default)
 * @param selected_action Output: selected action ID
 * @return 0 on success, negative on error
 */
int basal_ganglia_select_action_mcts(
    basal_ganglia_t* bg,
    const float* cortical_input,
    uint32_t num_iterations,
    uint32_t* selected_action
);

//=============================================================================
// Dopamine Modulation Functions
//=============================================================================

/**
 * @brief Update dopamine level based on reward
 * @param bg Basal ganglia system
 * @param reward Reward signal [-1, 1]
 * @param expected_reward Expected reward (for RPE computation)
 * @return 0 on success, negative on error
 */
int basal_ganglia_update_dopamine(
    basal_ganglia_t* bg,
    float reward,
    float expected_reward
);

/**
 * @brief Set dopamine level directly (for external modulation)
 * @param bg Basal ganglia system
 * @param level Dopamine level [0-1]
 * @return 0 on success, negative on error
 */
int basal_ganglia_set_dopamine(basal_ganglia_t* bg, float level);

/**
 * @brief Get current dopamine level
 * @param bg Basal ganglia system
 * @return Current dopamine level [0-1]
 */
float basal_ganglia_get_dopamine(const basal_ganglia_t* bg);

/**
 * @brief Get reward prediction error
 * @param bg Basal ganglia system
 * @return Current RPE value
 */
float basal_ganglia_get_rpe(const basal_ganglia_t* bg);

//=============================================================================
// Habit Learning Functions
//=============================================================================

/**
 * @brief Register a habit
 * @param bg Basal ganglia system
 * @param context Trigger context ID
 * @param action_id Associated action
 * @param habit_id Output: new habit ID
 * @return 0 on success, negative on error
 */
int basal_ganglia_register_habit(
    basal_ganglia_t* bg,
    uint32_t context,
    uint32_t action_id,
    uint32_t* habit_id
);

/**
 * @brief Strengthen habit through repetition
 * @param bg Basal ganglia system
 * @param habit_id Habit to strengthen
 * @param success Whether execution was successful
 * @return 0 on success, negative on error
 */
int basal_ganglia_strengthen_habit(
    basal_ganglia_t* bg,
    uint32_t habit_id,
    bool success
);

/**
 * @brief Check if context triggers a habit
 * @param bg Basal ganglia system
 * @param context Context to check
 * @param action_id Output: triggered action (if any)
 * @return true if habit triggered, false otherwise
 */
bool basal_ganglia_check_habit(
    const basal_ganglia_t* bg,
    uint32_t context,
    uint32_t* action_id
);

/**
 * @brief Get habit strength
 * @param bg Basal ganglia system
 * @param habit_id Habit to query
 * @return Habit strength [0-1], or -1 on error
 */
float basal_ganglia_get_habit_strength(
    const basal_ganglia_t* bg,
    uint32_t habit_id
);

/**
 * @brief Switch between goal-directed and habitual mode
 * @param bg Basal ganglia system
 * @param habitual True for habitual mode, false for goal-directed
 * @return 0 on success, negative on error
 */
int basal_ganglia_set_habit_mode(basal_ganglia_t* bg, bool habitual);

/**
 * @brief Check if currently in habit mode
 * @param bg Basal ganglia system
 * @return true if in habit mode
 */
bool basal_ganglia_is_habit_mode(const basal_ganglia_t* bg);

//=============================================================================
// Pathway Analysis Functions
//=============================================================================

/**
 * @brief Get direct pathway activation for action
 * @param bg Basal ganglia system
 * @param action_id Action to query
 * @return Direct pathway activation [0-1]
 */
float basal_ganglia_get_direct_activation(
    const basal_ganglia_t* bg,
    uint32_t action_id
);

/**
 * @brief Get indirect pathway activation for action
 * @param bg Basal ganglia system
 * @param action_id Action to query
 * @return Indirect pathway activation [0-1]
 */
float basal_ganglia_get_indirect_activation(
    const basal_ganglia_t* bg,
    uint32_t action_id
);

/**
 * @brief Get conflict level between competing actions
 * @param bg Basal ganglia system
 * @return Conflict level [0-1]
 */
float basal_ganglia_get_conflict(const basal_ganglia_t* bg);

/**
 * @brief Get current operating mode
 * @param bg Basal ganglia system
 * @return Current operating mode
 */
bg_operating_mode_t basal_ganglia_get_mode(const basal_ganglia_t* bg);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Step basal ganglia simulation
 * @param bg Basal ganglia system
 * @param dt Time step in milliseconds
 * @return 0 on success, negative on error
 */
int basal_ganglia_step(basal_ganglia_t* bg, float dt);

/**
 * @brief Process cortical input through all pathways
 * @param bg Basal ganglia system
 * @param cortical_input Input from cortex
 * @return 0 on success, negative on error
 */
int basal_ganglia_process_input(
    basal_ganglia_t* bg,
    const float* cortical_input
);

//=============================================================================
// Bio-Async Integration
//=============================================================================

/**
 * @brief Connect to bio-async router
 * @param bg Basal ganglia system
 * @return 0 on success, negative on error
 */
int basal_ganglia_connect_bio_async(basal_ganglia_t* bg);

/**
 * @brief Disconnect from bio-async router
 * @param bg Basal ganglia system
 * @return 0 on success, negative on error
 */
int basal_ganglia_disconnect_bio_async(basal_ganglia_t* bg);

/**
 * @brief Check if connected to bio-async
 * @param bg Basal ganglia system
 * @return true if connected
 */
bool basal_ganglia_is_bio_async_connected(const basal_ganglia_t* bg);

/**
 * @brief Process all pending bio-async messages
 * @param bg Basal ganglia system
 * @return Number of messages processed
 */
int basal_ganglia_process_bio_messages(basal_ganglia_t* bg);

//=============================================================================
// System Integration API
//=============================================================================

/* Forward declarations for integration types */
struct nimcp_health_agent;
typedef struct nimcp_health_agent nimcp_health_agent_t;
typedef struct bbb_system_struct* bbb_system_t;

/**
 * @brief Set health agent for basal ganglia heartbeats
 * @param agent Health agent (can be NULL to disable)
 */
void basal_ganglia_set_health_agent(nimcp_health_agent_t* agent);

/**
 * @brief Set BBB system for input validation
 * @param bbb BBB system handle (can be NULL to disable)
 */
void basal_ganglia_set_bbb(bbb_system_t bbb);

/**
 * @brief Set KG context for state persistence
 * @param kg_ctx Knowledge graph context (can be NULL to disable)
 */
void basal_ganglia_set_kg_context(void* kg_ctx);

//=============================================================================
// Statistics and Debugging
//=============================================================================

/**
 * @brief Get basal ganglia statistics
 * @param bg Basal ganglia system
 * @param stats Output statistics
 * @return 0 on success, negative on error
 */
int basal_ganglia_get_stats(
    const basal_ganglia_t* bg,
    basal_ganglia_stats_t* stats
);

/**
 * @brief Get operating mode name
 * @param mode Operating mode
 * @return Mode name string
 */
const char* basal_ganglia_mode_name(bg_operating_mode_t mode);

/**
 * @brief Get action state name
 * @param state Action state
 * @return State name string
 */
const char* basal_ganglia_action_state_name(action_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BASAL_GANGLIA_H */
