/**
 * @file nimcp_basal_ganglia_training_bridge.h
 * @brief Basal ganglia-training module plasticity bridge
 *
 * WHAT: Integration bridge between basal ganglia and training system
 * WHY:  Enable reinforcement learning and habit plasticity in BG circuits
 * HOW:  Dopamine RPE signals drive synaptic weight updates in striatum
 *
 * BIOLOGICAL BASIS:
 * - Dopamine RPE signals drive cortico-striatal plasticity
 * - D1 MSNs: LTP with positive RPE, LTD with negative RPE
 * - D2 MSNs: Opposite plasticity direction
 * - Habit formation involves DLS plasticity (dorsolateral striatum)
 * - Goal-directed learning involves DMS (dorsomedial striatum)
 *
 * LEARNING RULES:
 * 1. Three-factor learning: Pre activity × Post activity × DA signal
 * 2. Eligibility traces: Allow credit assignment over time
 * 3. Actor-critic: Striatum as actor, dopamine as critic signal
 *
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BASAL_GANGLIA_TRAINING_BRIDGE_H
#define NIMCP_BASAL_GANGLIA_TRAINING_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/subcortical/nimcp_basal_ganglia.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "middleware/training/nimcp_training_module.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define BGTR_MAX_ELIGIBILITY_TRACES 256  /**< Maximum eligibility traces */
#define BGTR_DEFAULT_LEARNING_RATE  0.01f /**< Default learning rate */
#define BGTR_DEFAULT_TRACE_DECAY    0.95f /**< Eligibility trace decay */
#define BGTR_DEFAULT_DA_SCALE       1.0f  /**< Dopamine scaling factor */
#define BGTR_HABIT_LEARNING_SCALE   0.5f  /**< Slower learning for habits */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Learning type
 */
typedef enum {
    BGTR_LEARN_ACTOR_CRITIC = 0,  /**< Actor-critic RL */
    BGTR_LEARN_THREE_FACTOR,      /**< Three-factor Hebbian */
    BGTR_LEARN_REWARD_MODULATED,  /**< Simple reward modulation */
    BGTR_LEARN_HABIT_FORMATION    /**< Habit-specific learning */
} bgtr_learning_type_t;

/**
 * @brief Pathway target for learning
 */
typedef enum {
    BGTR_TARGET_DIRECT = 0,       /**< Direct pathway (D1) */
    BGTR_TARGET_INDIRECT,         /**< Indirect pathway (D2) */
    BGTR_TARGET_BOTH,             /**< Both pathways */
    BGTR_TARGET_DMS,              /**< Dorsomedial (goal-directed) */
    BGTR_TARGET_DLS               /**< Dorsolateral (habits) */
} bgtr_pathway_target_t;

/**
 * @brief Plasticity event type
 */
typedef enum {
    BGTR_EVENT_REWARD = 0,        /**< Reward received */
    BGTR_EVENT_PUNISHMENT,        /**< Punishment received */
    BGTR_EVENT_ACTION_SELECTED,   /**< Action was selected */
    BGTR_EVENT_ACTION_COMPLETED,  /**< Action completed */
    BGTR_EVENT_PREDICTION_ERROR   /**< RPE computed */
} bgtr_plasticity_event_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Eligibility trace
 */
typedef struct {
    uint32_t action_id;           /**< Associated action */
    uint32_t context_id;          /**< Context (state) */
    float trace_value;            /**< Current trace strength */
    uint64_t creation_time_ms;    /**< When trace was created */
    bool is_active;               /**< Trace is active */
} bgtr_eligibility_trace_t;

/**
 * @brief Learning event
 */
typedef struct {
    bgtr_plasticity_event_t type; /**< Event type */
    uint32_t action_id;           /**< Related action */
    float value;                  /**< Event value (reward, RPE, etc.) */
    uint64_t timestamp_ms;        /**< Event timestamp */
} bgtr_learning_event_t;

/**
 * @brief Weight update result
 */
typedef struct {
    uint32_t action_id;           /**< Updated action */
    bgtr_pathway_target_t target; /**< Which pathway updated */
    float old_weight;             /**< Previous weight */
    float new_weight;             /**< New weight */
    float delta;                  /**< Weight change */
} bgtr_weight_update_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    bgtr_learning_type_t learning_type; /**< Learning algorithm */
    float learning_rate;          /**< Base learning rate */
    float d1_learning_rate;       /**< D1 pathway rate */
    float d2_learning_rate;       /**< D2 pathway rate */
    float trace_decay;            /**< Eligibility trace decay */
    float da_scaling;             /**< Dopamine scaling */
    float habit_rate_scale;       /**< Habit learning rate modifier */
    uint32_t max_traces;          /**< Maximum eligibility traces */
    bool enable_eligibility;      /**< Enable eligibility traces */
    bool enable_habit_learning;   /**< Enable habit plasticity */
    bool enable_d1_d2_asymmetry;  /**< D1/D2 have opposite learning */
} bgtr_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;       /**< Total weight updates */
    uint64_t d1_updates;          /**< D1 pathway updates */
    uint64_t d2_updates;          /**< D2 pathway updates */
    uint64_t reward_events;       /**< Reward events processed */
    uint64_t punishment_events;   /**< Punishment events processed */
    uint64_t traces_created;      /**< Eligibility traces created */
    uint64_t traces_triggered;    /**< Traces that triggered learning */
    float avg_weight_delta;       /**< Average weight change */
    float avg_rpe;                /**< Average RPE magnitude */
    float total_reward;           /**< Cumulative reward */
} bgtr_bridge_stats_t;

/**
 * @brief BG-Training bridge instance
 */
typedef struct bgtr_bridge {
    bridge_base_t base;           /**< MUST be first: base bridge infrastructure */

    /* Connected components */
    basal_ganglia_t* bg;          /**< Connected basal ganglia */
    nimcp_training_context_t* training; /**< Training context */

    /* Weights */
    nimcp_training_weights_t d1_weights; /**< D1 pathway weights */
    nimcp_training_weights_t d2_weights; /**< D2 pathway weights */
    nimcp_training_weights_t habit_weights; /**< Habit weights */
    uint32_t num_actions;         /**< Number of actions */

    /* Eligibility traces */
    bgtr_eligibility_trace_t* traces; /**< Eligibility traces */
    uint32_t num_traces;          /**< Active trace count */
    uint32_t max_traces;          /**< Maximum traces */

    /* Learning state */
    float last_rpe;               /**< Last reward prediction error */
    float last_reward;            /**< Last reward value */
    uint32_t last_action;         /**< Last selected action */
    uint64_t last_action_time;    /**< When last action was selected */

    /* Configuration */
    bgtr_bridge_config_t config;  /**< Configuration */

    /* Statistics */
    bgtr_bridge_stats_t stats;    /**< Runtime statistics *//**< Mutex for thread safety */
} bgtr_bridge_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @param config Configuration to initialize
 */
void bgtr_bridge_default_config(bgtr_bridge_config_t* config);

/**
 * @brief Create BG-training bridge
 * @param config Configuration (NULL for defaults)
 * @param num_actions Number of actions
 * @return Bridge instance or NULL on failure
 */
bgtr_bridge_t* bgtr_bridge_create(
    const bgtr_bridge_config_t* config,
    uint32_t num_actions
);

/**
 * @brief Destroy bridge
 * @param bridge Bridge to destroy
 */
void bgtr_bridge_destroy(bgtr_bridge_t* bridge);

/**
 * @brief Reset bridge state (keep weights)
 * @param bridge Bridge to reset
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_reset(bgtr_bridge_t* bridge);

/* ============================================================================
 * Connection Functions
 * ============================================================================ */

/**
 * @brief Connect basal ganglia to bridge
 * @param bridge Bridge instance
 * @param bg Basal ganglia to connect
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_connect_bg(bgtr_bridge_t* bridge, basal_ganglia_t* bg);

/**
 * @brief Connect training context
 * @param bridge Bridge instance
 * @param training Training context to connect
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_connect_training(
    bgtr_bridge_t* bridge,
    nimcp_training_context_t* training
);

/**
 * @brief Check if fully connected
 * @param bridge Bridge instance
 * @return true if BG connected (training optional)
 */
bool bgtr_bridge_is_connected(const bgtr_bridge_t* bridge);

/* ============================================================================
 * Learning Functions
 * ============================================================================ */

/**
 * @brief Record action selection (creates eligibility trace)
 * @param bridge Bridge instance
 * @param action_id Selected action
 * @param context_id Current context/state
 * @param current_time_ms Current time
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_record_action(
    bgtr_bridge_t* bridge,
    uint32_t action_id,
    uint32_t context_id,
    uint64_t current_time_ms
);

/**
 * @brief Process reward signal
 *
 * Computes RPE and triggers weight updates:
 * 1. Compute RPE = reward - expected
 * 2. Find active eligibility traces
 * 3. Update D1/D2 weights based on RPE sign
 *
 * @param bridge Bridge instance
 * @param reward Reward value
 * @param expected_reward Expected reward (for RPE)
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_process_reward(
    bgtr_bridge_t* bridge,
    float reward,
    float expected_reward
);

/**
 * @brief Update weights based on RPE
 *
 * Direct pathway: Δw = η × trace × RPE (positive RPE → LTP)
 * Indirect pathway: Δw = η × trace × (-RPE) (negative RPE → LTP)
 *
 * @param bridge Bridge instance
 * @param rpe Reward prediction error
 * @return Number of weights updated, -1 on error
 */
int bgtr_bridge_update_weights(bgtr_bridge_t* bridge, float rpe);

/**
 * @brief Decay eligibility traces
 * @param bridge Bridge instance
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_decay_traces(bgtr_bridge_t* bridge, float dt_ms);

/**
 * @brief Strengthen habit for action
 * @param bridge Bridge instance
 * @param action_id Action to strengthen
 * @param amount Strength increase
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_strengthen_habit(
    bgtr_bridge_t* bridge,
    uint32_t action_id,
    float amount
);

/* ============================================================================
 * Weight Access Functions
 * ============================================================================ */

/**
 * @brief Get D1 pathway weight for action
 * @param bridge Bridge instance
 * @param action_id Action to query
 * @return Weight value, or -1 on error
 */
float bgtr_bridge_get_d1_weight(
    const bgtr_bridge_t* bridge,
    uint32_t action_id
);

/**
 * @brief Get D2 pathway weight for action
 * @param bridge Bridge instance
 * @param action_id Action to query
 * @return Weight value, or -1 on error
 */
float bgtr_bridge_get_d2_weight(
    const bgtr_bridge_t* bridge,
    uint32_t action_id
);

/**
 * @brief Get net action value (D1 - D2)
 * @param bridge Bridge instance
 * @param action_id Action to query
 * @return Net value, or 0 on error
 */
float bgtr_bridge_get_action_value(
    const bgtr_bridge_t* bridge,
    uint32_t action_id
);

/**
 * @brief Set weight directly (for initialization)
 * @param bridge Bridge instance
 * @param action_id Action to set
 * @param target Which pathway
 * @param weight Weight value
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_set_weight(
    bgtr_bridge_t* bridge,
    uint32_t action_id,
    bgtr_pathway_target_t target,
    float weight
);

/* ============================================================================
 * Eligibility Trace Functions
 * ============================================================================ */

/**
 * @brief Get active trace count
 * @param bridge Bridge instance
 * @return Number of active traces
 */
uint32_t bgtr_bridge_get_trace_count(const bgtr_bridge_t* bridge);

/**
 * @brief Get trace for action
 * @param bridge Bridge instance
 * @param action_id Action to query
 * @return Trace value, or 0 if no trace
 */
float bgtr_bridge_get_trace(
    const bgtr_bridge_t* bridge,
    uint32_t action_id
);

/**
 * @brief Clear all eligibility traces
 * @param bridge Bridge instance
 */
void bgtr_bridge_clear_traces(bgtr_bridge_t* bridge);

/* ============================================================================
 * Checkpoint Functions
 * ============================================================================ */

/**
 * @brief Create checkpoint of current weights
 * @param bridge Bridge instance
 * @param checkpoint Output: checkpoint
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_checkpoint(
    bgtr_bridge_t* bridge,
    nimcp_training_checkpoint_t* checkpoint
);

/**
 * @brief Restore weights from checkpoint
 * @param bridge Bridge instance
 * @param checkpoint Checkpoint to restore
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_restore(
    bgtr_bridge_t* bridge,
    const nimcp_training_checkpoint_t* checkpoint
);

/* ============================================================================
 * Statistics Functions
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 * @param bridge Bridge instance
 * @param stats Output: statistics
 * @return 0 on success, -1 on error
 */
int bgtr_bridge_get_stats(
    const bgtr_bridge_t* bridge,
    bgtr_bridge_stats_t* stats
);

/**
 * @brief Reset statistics
 * @param bridge Bridge instance
 */
void bgtr_bridge_reset_stats(bgtr_bridge_t* bridge);

/**
 * @brief Get learning type name
 * @param type Learning type
 * @return Type name string
 */
const char* bgtr_learning_type_name(bgtr_learning_type_t type);

/**
 * @brief Get pathway target name
 * @param target Pathway target
 * @return Target name string
 */
const char* bgtr_pathway_target_name(bgtr_pathway_target_t target);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BASAL_GANGLIA_TRAINING_BRIDGE_H */
