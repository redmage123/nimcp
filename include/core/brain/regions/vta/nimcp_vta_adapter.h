/**
 * @file nimcp_vta_adapter.h
 * @brief VTA Adapter for brain integration
 * @version 1.1.0
 * @date 2026-01-11
 *
 * Provides integration layer for VTA with:
 * - Brain factory registration
 * - Bio-async messaging
 * - Inter-module communication
 * - Immune system integration
 * - BIDIRECTIONAL TRAINING LAYER INTEGRATION
 *
 * Training Integration:
 * - DA modulates motivation/vigor during training (approach behavior)
 * - RPE signals inform weight updates (TD learning)
 * - Goal progress affects DA release (incentive salience)
 * - Training rewards affect DA baseline and phasic responses
 */

#ifndef NIMCP_VTA_ADAPTER_H
#define NIMCP_VTA_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct nimcp_brain;
struct nimcp_bio_router;
struct nimcp_immune_system;
struct nimcp_vta_system;
struct nimcp_training_hub;
typedef struct nimcp_vta_system nimcp_vta_system_t;

/*=============================================================================
 * Constants
 *===========================================================================*/

#define VTA_ADAPTER_MAX_CALLBACKS  16
#define VTA_ADAPTER_MSG_QUEUE_SIZE 64

/*=============================================================================
 * Message Types
 *===========================================================================*/

typedef enum {
    VTA_MSG_REWARD = 0,           /**< Reward signal */
    VTA_MSG_RPE_UPDATE,           /**< RPE computed */
    VTA_MSG_BURST,                /**< DA burst triggered */
    VTA_MSG_PAUSE,                /**< DA pause triggered */
    VTA_MSG_MOTIVATION_UPDATE,    /**< Motivation changed */
    VTA_MSG_GOAL_ACHIEVED,        /**< Goal completed */
    VTA_MSG_CUE_DETECTED,         /**< Reward cue detected */
    VTA_MSG_DA_LEVEL,             /**< DA level update */
    VTA_MSG_MODE_CHANGED,         /**< Operating mode changed */
    VTA_MSG_COUNT
} nimcp_vta_msg_type_t;

/*=============================================================================
 * Message Data Structures
 *===========================================================================*/

typedef struct {
    float reward_magnitude;
    float rpe;
} nimcp_vta_reward_data_t;

typedef struct {
    float rpe;
    float expected;
    float actual;
    bool is_positive;
} nimcp_vta_rpe_data_t;

typedef struct {
    float intensity;
    float duration;
    float da_level;
} nimcp_vta_burst_data_t;

typedef struct {
    float wanting;
    float vigor;
    uint32_t goal_id;
} nimcp_vta_motivation_data_t;

typedef struct {
    float da_level;
    float baseline_ratio;
} nimcp_vta_da_data_t;

/**
 * @brief VTA message
 */
typedef struct {
    nimcp_vta_msg_type_t type;
    uint64_t timestamp;
    union {
        nimcp_vta_reward_data_t reward;
        nimcp_vta_rpe_data_t rpe;
        nimcp_vta_burst_data_t burst;
        nimcp_vta_motivation_data_t motivation;
        nimcp_vta_da_data_t da;
        float generic_value;
    } data;
} nimcp_vta_message_t;

/*=============================================================================
 * Training Integration Types
 *===========================================================================*/

/**
 * @brief Training event types that VTA responds to
 */
typedef enum {
    VTA_TRAIN_EVENT_LOSS = 0,          /**< Loss value reported */
    VTA_TRAIN_EVENT_REWARD,            /**< Training reward signal */
    VTA_TRAIN_EVENT_IMPROVEMENT,       /**< Performance improvement */
    VTA_TRAIN_EVENT_EPOCH_START,       /**< New epoch started */
    VTA_TRAIN_EVENT_EPOCH_END,         /**< Epoch completed */
    VTA_TRAIN_EVENT_GOAL_PROGRESS,     /**< Progress toward goal */
    VTA_TRAIN_EVENT_MILESTONE,         /**< Training milestone reached */
    VTA_TRAIN_EVENT_COUNT
} nimcp_vta_train_event_t;

/**
 * @brief Training state received from training layer
 */
typedef struct {
    float current_loss;
    float loss_improvement;            /**< Improvement from last (negative is better) */
    float current_reward;              /**< RL reward signal */
    float expected_reward;             /**< Expected reward (for RPE) */
    uint32_t current_epoch;
    float goal_progress;               /**< 0-1 progress toward goal */
    float training_time;               /**< Total training time (s) */
} nimcp_vta_training_state_t;

/**
 * @brief Modulation signal sent to training layer
 * DA influences motivation, reward weighting, and learning
 */
typedef struct {
    float lr_multiplier;               /**< Learning rate modulation [0.5-2.0] */
    float reward_sensitivity;          /**< Sensitivity to rewards [0.5-2.0] */
    float motivation_signal;           /**< Vigor/approach signal [0-1] */
    float persistence_factor;          /**< Continue despite difficulty [0-1] */
    float rpe_signal;                  /**< Reward prediction error */
    bool suggest_exploration;          /**< Suggest exploration over exploitation */
    bool suggest_checkpoint;           /**< Suggest saving (milestone reached) */
} nimcp_vta_training_modulation_t;

/**
 * @brief Training callback for bidirectional communication
 */
typedef void (*nimcp_vta_training_callback_fn)(
    const nimcp_vta_training_modulation_t* modulation,
    void* user_data
);

/*=============================================================================
 * Callback Types
 *===========================================================================*/

typedef struct nimcp_vta_adapter* nimcp_vta_adapter_t;

typedef void (*nimcp_vta_callback_fn)(
    nimcp_vta_adapter_t adapter,
    const nimcp_vta_message_t* msg,
    void* user_data
);

/*=============================================================================
 * Configuration
 *===========================================================================*/

typedef struct {
    bool enable_bio_async;        /**< Enable bio-async messaging */
    bool auto_create_projections; /**< Auto-create standard projections */
    bool connect_to_nac;          /**< Connect to nucleus accumbens */
    bool connect_to_pfc;          /**< Connect to prefrontal cortex */
    float message_rate_limit;     /**< Max messages per second */

    /* Training integration config */
    bool enable_training_integration;   /**< Enable training layer bridge */
    float reward_lr_boost;              /**< How much reward boosts LR [1.0-2.0] */
    float motivation_persistence_scale; /**< How motivation affects persistence */
    float rpe_sensitivity;              /**< Sensitivity to RPE signals */
} nimcp_vta_adapter_config_t;

/**
 * @brief Get default adapter configuration
 */
nimcp_vta_adapter_config_t nimcp_vta_adapter_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create VTA adapter
 */
nimcp_vta_adapter_t nimcp_vta_adapter_create(const nimcp_vta_adapter_config_t* config);

/**
 * @brief Destroy VTA adapter
 */
void nimcp_vta_adapter_destroy(nimcp_vta_adapter_t adapter);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect adapter to brain
 */
int nimcp_vta_adapter_connect_brain(
    nimcp_vta_adapter_t adapter,
    struct nimcp_brain* brain
);

/**
 * @brief Disconnect from brain
 */
int nimcp_vta_adapter_disconnect(nimcp_vta_adapter_t adapter);

/**
 * @brief Set bio-async router
 */
int nimcp_vta_adapter_set_router(
    nimcp_vta_adapter_t adapter,
    struct nimcp_bio_router* router
);

/**
 * @brief Connect to immune system
 */
int nimcp_vta_adapter_connect_immune(
    nimcp_vta_adapter_t adapter,
    struct nimcp_immune_system* immune
);

/*=============================================================================
 * VTA Access API
 *===========================================================================*/

/**
 * @brief Get internal VTA system
 */
nimcp_vta_system_t* nimcp_vta_adapter_get_vta(nimcp_vta_adapter_t adapter);

/*=============================================================================
 * Messaging API
 *===========================================================================*/

/**
 * @brief Send message
 */
int nimcp_vta_adapter_send_message(
    nimcp_vta_adapter_t adapter,
    const nimcp_vta_message_t* msg
);

/**
 * @brief Process pending messages
 */
int nimcp_vta_adapter_process_messages(
    nimcp_vta_adapter_t adapter,
    int max_messages
);

/**
 * @brief Register callback for message type
 */
int nimcp_vta_adapter_register_callback(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_msg_type_t type,
    nimcp_vta_callback_fn callback,
    void* user_data
);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update adapter (includes VTA update)
 */
int nimcp_vta_adapter_update(nimcp_vta_adapter_t adapter, float dt);

/*=============================================================================
 * Reward Processing API
 *===========================================================================*/

/**
 * @brief Process reward signal
 */
int nimcp_vta_adapter_process_reward(
    nimcp_vta_adapter_t adapter,
    float reward_magnitude
);

/**
 * @brief Process cue detection
 */
int nimcp_vta_adapter_process_cue(
    nimcp_vta_adapter_t adapter,
    uint32_t cue_id,
    float predictive_value
);

/**
 * @brief Process goal state
 */
int nimcp_vta_adapter_process_goal(
    nimcp_vta_adapter_t adapter,
    uint32_t goal_id,
    float value,
    float effort,
    float distance
);

/*=============================================================================
 * Integration API
 *===========================================================================*/

/**
 * @brief Process immune signal
 */
int nimcp_vta_adapter_process_immune(
    nimcp_vta_adapter_t adapter,
    float inflammation,
    const float* cytokines,
    uint32_t num_cytokines
);

/**
 * @brief Apply prefrontal modulation
 */
int nimcp_vta_adapter_apply_pfc_modulation(
    nimcp_vta_adapter_t adapter,
    float inhibition
);

/**
 * @brief Apply habenula inhibition (negative RPE)
 */
int nimcp_vta_adapter_apply_habenula_inhibition(
    nimcp_vta_adapter_t adapter,
    float inhibition
);

/*=============================================================================
 * Training Integration API (BIDIRECTIONAL)
 *===========================================================================*/

/**
 * @brief Connect to training hub (BIDIRECTIONAL)
 * @param adapter Adapter handle
 * @param training_hub Training hub instance
 * @return 0 on success
 */
int nimcp_vta_adapter_connect_training(
    nimcp_vta_adapter_t adapter,
    struct nimcp_training_hub* training_hub
);

/**
 * @brief Receive training event (from training layer -> VTA)
 * Updates DA/motivation based on training state (rewards, progress, etc.)
 * @param adapter Adapter handle
 * @param event Event type
 * @param state Current training state
 * @return 0 on success
 */
int nimcp_vta_adapter_on_training_event(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_train_event_t event,
    const nimcp_vta_training_state_t* state
);

/**
 * @brief Get current training modulation (VTA -> training layer)
 * Returns current DA-based modulation signals for training
 * @param adapter Adapter handle
 * @param modulation Output modulation values
 * @return 0 on success
 */
int nimcp_vta_adapter_get_training_modulation(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_training_modulation_t* modulation
);

/**
 * @brief Register callback for training modulation updates
 * Training layer registers to receive modulation signals
 * @param adapter Adapter handle
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success
 */
int nimcp_vta_adapter_register_training_callback(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_training_callback_fn callback,
    void* user_data
);

/**
 * @brief Compute RPE for training
 * Used by training layer to compute reward prediction error
 * @param adapter Adapter handle
 * @param expected Expected reward
 * @param received Received reward
 * @param rpe Output RPE value
 * @return 0 on success
 */
int nimcp_vta_adapter_compute_training_rpe(
    nimcp_vta_adapter_t adapter,
    float expected,
    float received,
    float* rpe
);

/**
 * @brief Process training reward signal
 * @param adapter Adapter handle
 * @param reward Reward magnitude
 * @return 0 on success
 */
int nimcp_vta_adapter_process_training_reward(
    nimcp_vta_adapter_t adapter,
    float reward
);

/**
 * @brief Process goal progress update
 * @param adapter Adapter handle
 * @param progress Progress toward goal [0-1]
 * @return 0 on success
 */
int nimcp_vta_adapter_process_goal_progress(
    nimcp_vta_adapter_t adapter,
    float progress
);

/*=============================================================================
 * State API
 *===========================================================================*/

typedef struct {
    bool is_active;
    float da_level;
    float current_rpe;
    float motivation;
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t updates_processed;
    float total_active_time;
    /* Training integration stats */
    uint32_t training_events_received;
    uint32_t training_modulations_sent;
} nimcp_vta_adapter_state_t;

/**
 * @brief Get adapter state
 */
int nimcp_vta_adapter_get_state(
    nimcp_vta_adapter_t adapter,
    nimcp_vta_adapter_state_t* state
);

/**
 * @brief Reset adapter statistics
 */
int nimcp_vta_adapter_reset_stats(nimcp_vta_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VTA_ADAPTER_H */
