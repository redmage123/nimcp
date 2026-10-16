/**
 * @file nimcp_raphe_adapter.h
 * @brief Raphe Adapter for brain and training layer integration
 * @date 2026-01-11
 *
 * Provides bidirectional integration layer for Raphe with:
 * - Brain factory registration
 * - Bio-async messaging
 * - Inter-module communication
 * - Immune system integration
 * - BIDIRECTIONAL TRAINING LAYER INTEGRATION
 *
 * Training Integration:
 * - 5-HT modulates impulse control during training (patience for delayed rewards)
 * - Training stress affects mood/5-HT levels
 * - Temporal discounting influences curriculum pacing
 * - Mood state affects learning rate and exploration
 */

#ifndef NIMCP_RAPHE_ADAPTER_H
#define NIMCP_RAPHE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* Forward declarations */
struct nimcp_brain;
struct nimcp_bio_router;
struct nimcp_immune_system;
struct nimcp_raphe_system;
struct nimcp_training_hub;
typedef struct nimcp_raphe_system nimcp_raphe_system_t;

/*=============================================================================
 * Constants
 *===========================================================================*/

#define RAPHE_ADAPTER_MAX_CALLBACKS     16
#define RAPHE_ADAPTER_MSG_QUEUE_SIZE    64

/*=============================================================================
 * Message Types
 *===========================================================================*/

typedef enum {
    RAPHE_MSG_5HT_LEVEL = 0,          /**< 5-HT level update */
    RAPHE_MSG_MOOD_UPDATE,            /**< Mood valence changed */
    RAPHE_MSG_ANXIETY_UPDATE,         /**< Anxiety level changed */
    RAPHE_MSG_IMPULSE_SIGNAL,         /**< Impulse control signal */
    RAPHE_MSG_PATIENCE_UPDATE,        /**< Patience/delay tolerance changed */
    RAPHE_MSG_SLEEP_PRESSURE,         /**< Sleep pressure update */
    RAPHE_MSG_MODE_CHANGED,           /**< Operating mode changed */
    RAPHE_MSG_TRAINING_MODULATION,    /**< Training modulation signal */
    RAPHE_MSG_COUNT
} nimcp_raphe_msg_type_t;

/*=============================================================================
 * Message Data Structures
 *===========================================================================*/

typedef struct {
    float ht_level;
    float baseline_ratio;
} nimcp_raphe_ht_data_t;

typedef struct {
    float valence;
    float stability;
    int mood_state;
} nimcp_raphe_mood_data_t;

typedef struct {
    float anxiety;
    float irritability;
} nimcp_raphe_anxiety_data_t;

typedef struct {
    float inhibition_strength;
    float patience;
    float impulsivity;
} nimcp_raphe_impulse_data_t;

typedef struct {
    float learning_rate_mod;       /**< Modulation factor for LR */
    float exploration_mod;         /**< Modulation for exploration */
    float patience_signal;         /**< Patience for delayed rewards */
    float impulse_control;         /**< Inhibit impulsive actions */
} nimcp_raphe_training_data_t;

/**
 * @brief Raphe message
 */
typedef struct {
    nimcp_raphe_msg_type_t type;
    uint64_t timestamp;
    union {
        nimcp_raphe_ht_data_t ht;
        nimcp_raphe_mood_data_t mood;
        nimcp_raphe_anxiety_data_t anxiety;
        nimcp_raphe_impulse_data_t impulse;
        nimcp_raphe_training_data_t training;
        float generic_value;
    } data;
} nimcp_raphe_message_t;

/*=============================================================================
 * Training Integration Types
 *===========================================================================*/

/**
 * @brief Training event types that Raphe responds to
 */
typedef enum {
    RAPHE_TRAIN_EVENT_LOSS = 0,        /**< Loss value reported */
    RAPHE_TRAIN_EVENT_GRADIENT,        /**< Gradient magnitude reported */
    RAPHE_TRAIN_EVENT_LR_CHANGE,       /**< Learning rate changed */
    RAPHE_TRAIN_EVENT_EPOCH_START,     /**< New epoch started */
    RAPHE_TRAIN_EVENT_EPOCH_END,       /**< Epoch completed */
    RAPHE_TRAIN_EVENT_REWARD,          /**< Reward signal from RL */
    RAPHE_TRAIN_EVENT_TIMEOUT,         /**< Training timeout/patience */
    RAPHE_TRAIN_EVENT_COUNT
} nimcp_raphe_train_event_t;

/**
 * @brief Training state received from training layer
 */
typedef struct {
    float current_loss;
    float loss_trend;                  /**< Improving (-) or worsening (+) */
    float gradient_norm;
    float current_lr;
    uint32_t current_epoch;
    float training_time;               /**< Total training time (s) */
    float progress;                    /**< 0-1 progress to goal */
} nimcp_raphe_training_state_t;

/**
 * @brief Modulation signal sent to training layer
 */
typedef struct {
    float lr_multiplier;               /**< Learning rate modulation [0.5-2.0] */
    float exploration_rate;            /**< Exploration vs exploitation [0-1] */
    float patience_factor;             /**< Patience for convergence [0-1] */
    float impulse_inhibition;          /**< Inhibit sudden changes [0-1] */
    bool suggest_consolidation;        /**< Suggest memory consolidation */
    bool suggest_break;                /**< Suggest training break */
} nimcp_raphe_training_modulation_t;

/**
 * @brief Training callback for bidirectional communication
 */
typedef void (*nimcp_raphe_training_callback_fn)(
    const nimcp_raphe_training_modulation_t* modulation,
    void* user_data
);

/*=============================================================================
 * Callback Types
 *===========================================================================*/

typedef struct nimcp_raphe_adapter* nimcp_raphe_adapter_t;

typedef void (*nimcp_raphe_callback_fn)(
    nimcp_raphe_adapter_t adapter,
    const nimcp_raphe_message_t* msg,
    void* user_data
);

/*=============================================================================
 * Configuration
 *===========================================================================*/

typedef struct {
    bool enable_bio_async;            /**< Enable bio-async messaging */
    bool auto_create_projections;     /**< Auto-create standard projections */
    bool enable_training_integration; /**< Enable training layer bridge */
    float message_rate_limit;         /**< Max messages per second */

    /* Training integration config */
    float loss_stress_sensitivity;    /**< How much loss affects mood */
    float gradient_stress_threshold;  /**< Gradient norm for stress response */
    float patience_decay_rate;        /**< How patience decays during training */
} nimcp_raphe_adapter_config_t;

/**
 * @brief Get default adapter configuration
 */
nimcp_raphe_adapter_config_t nimcp_raphe_adapter_default_config(void);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * @brief Create Raphe adapter
 */
nimcp_raphe_adapter_t nimcp_raphe_adapter_create(const nimcp_raphe_adapter_config_t* config);

/**
 * @brief Destroy Raphe adapter
 */
void nimcp_raphe_adapter_destroy(nimcp_raphe_adapter_t adapter);

/*=============================================================================
 * Connection API
 *===========================================================================*/

/**
 * @brief Connect adapter to brain
 */
int nimcp_raphe_adapter_connect_brain(
    nimcp_raphe_adapter_t adapter,
    struct nimcp_brain* brain
);

/**
 * @brief Disconnect from brain
 */
int nimcp_raphe_adapter_disconnect(nimcp_raphe_adapter_t adapter);

/**
 * @brief Set bio-async router
 */
int nimcp_raphe_adapter_set_router(
    nimcp_raphe_adapter_t adapter,
    struct nimcp_bio_router* router
);

/**
 * @brief Connect to immune system
 */
int nimcp_raphe_adapter_connect_immune(
    nimcp_raphe_adapter_t adapter,
    struct nimcp_immune_system* immune
);

/**
 * @brief Connect to training hub (BIDIRECTIONAL)
 */
int nimcp_raphe_adapter_connect_training(
    nimcp_raphe_adapter_t adapter,
    struct nimcp_training_hub* training_hub
);

/*=============================================================================
 * Raphe Access API
 *===========================================================================*/

/**
 * @brief Get internal Raphe system
 */
nimcp_raphe_system_t* nimcp_raphe_adapter_get_raphe(nimcp_raphe_adapter_t adapter);

/*=============================================================================
 * Messaging API
 *===========================================================================*/

/**
 * @brief Send message
 */
int nimcp_raphe_adapter_send_message(
    nimcp_raphe_adapter_t adapter,
    const nimcp_raphe_message_t* msg
);

/**
 * @brief Process pending messages
 */
int nimcp_raphe_adapter_process_messages(
    nimcp_raphe_adapter_t adapter,
    int max_messages
);

/**
 * @brief Register callback for message type
 */
int nimcp_raphe_adapter_register_callback(
    nimcp_raphe_adapter_t adapter,
    nimcp_raphe_msg_type_t type,
    nimcp_raphe_callback_fn callback,
    void* user_data
);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * @brief Update adapter (includes Raphe update)
 */
int nimcp_raphe_adapter_update(nimcp_raphe_adapter_t adapter, float dt);

/*=============================================================================
 * Training Integration API (BIDIRECTIONAL)
 *===========================================================================*/

/**
 * @brief Receive training event (from training layer -> Raphe)
 * Updates 5-HT/mood based on training state
 */
int nimcp_raphe_adapter_on_training_event(
    nimcp_raphe_adapter_t adapter,
    nimcp_raphe_train_event_t event,
    const nimcp_raphe_training_state_t* state
);

/**
 * @brief Get current training modulation (Raphe -> training layer)
 * Returns current 5-HT-based modulation signals for training
 */
int nimcp_raphe_adapter_get_training_modulation(
    nimcp_raphe_adapter_t adapter,
    nimcp_raphe_training_modulation_t* modulation
);

/**
 * @brief Register callback for training modulation updates
 * Training layer registers to receive modulation signals
 */
int nimcp_raphe_adapter_register_training_callback(
    nimcp_raphe_adapter_t adapter,
    nimcp_raphe_training_callback_fn callback,
    void* user_data
);

/**
 * @brief Compute impulse control for training action
 * Used by training layer to check if action should be inhibited
 */
int nimcp_raphe_adapter_compute_impulse_control(
    nimcp_raphe_adapter_t adapter,
    float action_urgency,
    float* inhibition_signal
);

/**
 * @brief Compute temporal discount for training reward
 * Used by RL training to discount future rewards
 */
int nimcp_raphe_adapter_compute_reward_discount(
    nimcp_raphe_adapter_t adapter,
    float reward,
    float delay,
    float* discounted_reward
);

/*=============================================================================
 * Mood/Anxiety Processing API
 *===========================================================================*/

/**
 * @brief Process stress signal (e.g., from high loss)
 */
int nimcp_raphe_adapter_process_stress(
    nimcp_raphe_adapter_t adapter,
    float stress_level
);

/**
 * @brief Process positive feedback (e.g., loss improvement)
 */
int nimcp_raphe_adapter_process_positive_feedback(
    nimcp_raphe_adapter_t adapter,
    float feedback_magnitude
);

/*=============================================================================
 * Integration API
 *===========================================================================*/

/**
 * @brief Process immune signal
 */
int nimcp_raphe_adapter_process_immune(
    nimcp_raphe_adapter_t adapter,
    float inflammation,
    const float* cytokines,
    uint32_t num_cytokines
);

/**
 * @brief Apply VTA DA modulation
 */
int nimcp_raphe_adapter_apply_vta_modulation(
    nimcp_raphe_adapter_t adapter,
    float da_level
);

/**
 * @brief Apply habenula inhibition
 */
int nimcp_raphe_adapter_apply_habenula_input(
    nimcp_raphe_adapter_t adapter,
    float input
);

/*=============================================================================
 * State API
 *===========================================================================*/

typedef struct {
    bool is_active;
    float ht_level;
    float mood_valence;
    float anxiety;
    float patience;
    uint32_t messages_sent;
    uint32_t messages_received;
    uint32_t updates_processed;
    float total_active_time;
    /* Training integration stats */
    uint32_t training_events_received;
    uint32_t training_modulations_sent;
} nimcp_raphe_adapter_state_t;

/**
 * @brief Get adapter state
 */
int nimcp_raphe_adapter_get_state(
    nimcp_raphe_adapter_t adapter,
    nimcp_raphe_adapter_state_t* state
);

/**
 * @brief Reset adapter statistics
 */
int nimcp_raphe_adapter_reset_stats(nimcp_raphe_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RAPHE_ADAPTER_H */
