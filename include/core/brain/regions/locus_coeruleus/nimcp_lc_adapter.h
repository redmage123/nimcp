/**
 * @file nimcp_lc_adapter.h
 * @brief Locus Coeruleus Adapter for Brain Integration
 * @version 1.1.0
 * @date 2026-01-11
 *
 * WHAT: Adapter layer for integrating LC with brain factory and other modules
 * WHY:  Provide standardized interface for LC within brain architecture
 * HOW:  Wrap LC core functionality, expose brain-compatible interface
 *
 * INTEGRATION POINTS:
 * - Bio-async messaging system
 * - Brain factory for initialization
 * - Immune system for stress response
 * - Thalamic relay for arousal gating
 * - Cortical modules for attention modulation
 * - BIDIRECTIONAL TRAINING LAYER INTEGRATION
 *
 * Training Integration:
 * - NE modulates attention/alertness during training (focus on relevant patterns)
 * - Training stress/novelty affects NE levels (phasic bursts on surprises)
 * - Arousal state affects learning rate and feature gain
 * - Vigilance influences training exploration vs exploitation
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LC_ADAPTER_H
#define NIMCP_LC_ADAPTER_H

#include "nimcp_locus_coeruleus.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Forward Declarations (Brain Integration Types)
//=============================================================================

/* These would be defined in brain core headers */
typedef struct nimcp_brain_s nimcp_brain_t;
typedef struct nimcp_bio_router_s nimcp_bio_router_t;
typedef struct nimcp_bio_async_handler_s nimcp_bio_async_handler_t;
typedef struct nimcp_immune_sensor_s nimcp_immune_sensor_t;
typedef struct nimcp_module_interface_s nimcp_module_interface_t;
struct nimcp_training_hub;  /* Forward declaration for training layer */

//=============================================================================
// Adapter Configuration
//=============================================================================

/**
 * @brief LC adapter configuration
 */
typedef struct {
    /* LC configuration */
    nimcp_lc_config_t lc_config;        /**< Core LC configuration */

    /* Integration options */
    bool enable_bio_async;              /**< Enable bio-async messaging */
    bool enable_immune_sensing;         /**< Enable immune integration */
    bool enable_thalamic_relay;         /**< Enable thalamic connection */
    bool enable_cortical_modulation;    /**< Enable cortical connections */

    /* Bio-async settings */
    uint32_t message_queue_size;        /**< Message queue capacity */
    float message_timeout_ms;           /**< Message handling timeout */

    /* Projection auto-setup */
    bool auto_create_projections;       /**< Auto-create standard projections */

    /* Logging */
    bool enable_adapter_logging;        /**< Enable adapter-level logging */
    const char* log_prefix;             /**< Log message prefix */

    /* Training integration config */
    bool enable_training_integration;   /**< Enable training layer bridge */
    float novelty_lr_boost;             /**< How much novelty boosts LR [1.0-2.0] */
    float stress_attention_boost;       /**< How stress affects attention gain */
    float arousal_exploration_scale;    /**< How arousal affects exploration */
} nimcp_lc_adapter_config_t;

/**
 * @brief LC adapter state
 */
typedef struct {
    /* Core status */
    bool is_active;                     /**< Adapter is active */
    bool is_connected;                  /**< Connected to brain */
    bool is_processing;                 /**< Currently processing */

    /* Connection status */
    bool bio_async_connected;           /**< Bio-async connected */
    bool immune_connected;              /**< Immune system connected */
    bool thalamic_connected;            /**< Thalamic relay connected */

    /* Statistics */
    uint64_t messages_received;         /**< Messages received */
    uint64_t messages_sent;             /**< Messages sent */
    uint64_t updates_processed;         /**< Update calls */
    float total_active_time;            /**< Time active (ms) */

    /* Training integration stats */
    uint32_t training_events_received;  /**< Training events processed */
    uint32_t training_modulations_sent; /**< Modulation signals sent */
} nimcp_lc_adapter_state_t;

//=============================================================================
// Bio-Async Message Types
//=============================================================================

/**
 * @brief LC message types for bio-async
 */
typedef enum {
    LC_MSG_NOVELTY_DETECTED = 0,        /**< Novelty event */
    LC_MSG_MODE_CHANGED,                /**< Tonic/phasic mode change */
    LC_MSG_AROUSAL_UPDATE,              /**< Arousal level update */
    LC_MSG_NE_RELEASE,                  /**< NE release event */
    LC_MSG_ATTENTION_RESET,             /**< Attention reset triggered */
    LC_MSG_STRESS_RESPONSE,             /**< Stress response activated */
    LC_MSG_REQUEST_INPUT,               /**< Request for input */
    LC_MSG_COUNT
} nimcp_lc_msg_type_t;

/**
 * @brief LC message payload
 */
typedef struct {
    nimcp_lc_msg_type_t type;           /**< Message type */
    float timestamp;                    /**< Message timestamp */

    union {
        struct {
            float novelty_score;
            float surprise_magnitude;
            bool triggered_burst;
        } novelty;

        struct {
            nimcp_lc_mode_t old_mode;
            nimcp_lc_mode_t new_mode;
        } mode_change;

        struct {
            float arousal_level;
            float alertness;
            float vigilance;
        } arousal;

        struct {
            nimcp_lc_target_t target;
            float ne_concentration;
            float gain_modulation;
        } ne_release;

        struct {
            float stress_level;
            float response_intensity;
        } stress;
    } data;
} nimcp_lc_message_t;

//=============================================================================
// Training Integration Types
//=============================================================================

/**
 * @brief Training event types that LC responds to
 */
typedef enum {
    LC_TRAIN_EVENT_LOSS = 0,           /**< Loss value reported */
    LC_TRAIN_EVENT_GRADIENT,           /**< Gradient magnitude reported */
    LC_TRAIN_EVENT_LR_CHANGE,          /**< Learning rate changed */
    LC_TRAIN_EVENT_EPOCH_START,        /**< New epoch started */
    LC_TRAIN_EVENT_EPOCH_END,          /**< Epoch completed */
    LC_TRAIN_EVENT_NOVELTY,            /**< Novel pattern detected */
    LC_TRAIN_EVENT_DIFFICULTY,         /**< Task difficulty changed */
    LC_TRAIN_EVENT_COUNT
} nimcp_lc_train_event_t;

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
    float difficulty;                  /**< Current task difficulty [0-1] */
    float novelty_score;               /**< Novelty of current batch [0-1] */
} nimcp_lc_training_state_t;

/**
 * @brief Modulation signal sent to training layer
 * NE influences attention, gain, and arousal for learning
 */
typedef struct {
    float lr_multiplier;               /**< Learning rate modulation [0.5-2.0] */
    float attention_gain;              /**< Feature gain modulation [0.5-2.0] */
    float exploration_factor;          /**< Exploration vs exploitation [0-1] */
    float vigilance_level;             /**< Pattern matching strictness [0-1] */
    bool suggest_attention_reset;      /**< Suggest resetting attention */
    bool suggest_save_state;           /**< Suggest checkpointing */
} nimcp_lc_training_modulation_t;

/**
 * @brief Training callback for bidirectional communication
 */
typedef void (*nimcp_lc_training_callback_fn)(
    const nimcp_lc_training_modulation_t* modulation,
    void* user_data
);

//=============================================================================
// Adapter Handle
//=============================================================================

typedef struct nimcp_lc_adapter_struct* nimcp_lc_adapter_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default adapter configuration
 */
NIMCP_EXPORT nimcp_lc_adapter_config_t nimcp_lc_adapter_default_config(void);

/**
 * @brief Create LC adapter
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
NIMCP_EXPORT nimcp_lc_adapter_t nimcp_lc_adapter_create(
    const nimcp_lc_adapter_config_t* config
);

/**
 * @brief Destroy LC adapter
 * @param adapter Adapter to destroy
 */
NIMCP_EXPORT void nimcp_lc_adapter_destroy(nimcp_lc_adapter_t adapter);

/**
 * @brief Connect adapter to brain
 * @param adapter Adapter handle
 * @param brain Brain instance
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_connect_brain(
    nimcp_lc_adapter_t adapter,
    nimcp_brain_t* brain
);

/**
 * @brief Disconnect from brain
 * @param adapter Adapter handle
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_disconnect(nimcp_lc_adapter_t adapter);

//=============================================================================
// Module Interface API
//=============================================================================

/**
 * @brief Get module interface for brain registration
 * @param adapter Adapter handle
 * @return Module interface (owned by adapter)
 */
NIMCP_EXPORT nimcp_module_interface_t* nimcp_lc_adapter_get_interface(
    nimcp_lc_adapter_t adapter
);

/**
 * @brief Get underlying LC system
 * @param adapter Adapter handle
 * @return LC system pointer (owned by adapter)
 */
NIMCP_EXPORT nimcp_lc_system_t* nimcp_lc_adapter_get_lc(
    nimcp_lc_adapter_t adapter
);

//=============================================================================
// Bio-Async API
//=============================================================================

/**
 * @brief Set bio-async router
 * @param adapter Adapter handle
 * @param router Bio-async router
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_set_router(
    nimcp_lc_adapter_t adapter,
    nimcp_bio_router_t* router
);

/**
 * @brief Send message via bio-async
 * @param adapter Adapter handle
 * @param message Message to send
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_send_message(
    nimcp_lc_adapter_t adapter,
    const nimcp_lc_message_t* message
);

/**
 * @brief Receive and process pending messages
 * @param adapter Adapter handle
 * @param max_messages Maximum messages to process
 * @return Number of messages processed
 */
NIMCP_EXPORT int nimcp_lc_adapter_process_messages(
    nimcp_lc_adapter_t adapter,
    uint32_t max_messages
);

/**
 * @brief Register message callback
 * @param adapter Adapter handle
 * @param msg_type Message type to handle
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success
 */
typedef void (*nimcp_lc_msg_callback_t)(
    nimcp_lc_adapter_t adapter,
    const nimcp_lc_message_t* message,
    void* user_data
);

NIMCP_EXPORT int nimcp_lc_adapter_register_callback(
    nimcp_lc_adapter_t adapter,
    nimcp_lc_msg_type_t msg_type,
    nimcp_lc_msg_callback_t callback,
    void* user_data
);

//=============================================================================
// Integration API
//=============================================================================

/**
 * @brief Connect to immune system
 * @param adapter Adapter handle
 * @param sensor Immune sensor
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_connect_immune(
    nimcp_lc_adapter_t adapter,
    nimcp_immune_sensor_t* sensor
);

/**
 * @brief Process immune system signals
 * @param adapter Adapter handle
 * @param inflammation_level Current inflammation (0-1)
 * @param cytokine_levels Array of cytokine levels
 * @param num_cytokines Number of cytokine values
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_process_immune(
    nimcp_lc_adapter_t adapter,
    float inflammation_level,
    const float* cytokine_levels,
    uint32_t num_cytokines
);

/**
 * @brief Apply thalamic gating
 * @param adapter Adapter handle
 * @param gate_level Gating factor (0-1)
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_apply_thalamic_gate(
    nimcp_lc_adapter_t adapter,
    float gate_level
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update adapter (single timestep)
 * @param adapter Adapter handle
 * @param dt Time delta (ms)
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_update(
    nimcp_lc_adapter_t adapter,
    float dt
);

/**
 * @brief Process sensory input
 * @param adapter Adapter handle
 * @param input Input array
 * @param input_size Size of input
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_process_input(
    nimcp_lc_adapter_t adapter,
    const float* input,
    uint32_t input_size
);

//=============================================================================
// Training Integration API (BIDIRECTIONAL)
//=============================================================================

/**
 * @brief Connect to training hub (BIDIRECTIONAL)
 * @param adapter Adapter handle
 * @param training_hub Training hub instance
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_connect_training(
    nimcp_lc_adapter_t adapter,
    struct nimcp_training_hub* training_hub
);

/**
 * @brief Receive training event (from training layer -> LC)
 * Updates NE/arousal based on training state (novelty, difficulty, etc.)
 * @param adapter Adapter handle
 * @param event Event type
 * @param state Current training state
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_on_training_event(
    nimcp_lc_adapter_t adapter,
    nimcp_lc_train_event_t event,
    const nimcp_lc_training_state_t* state
);

/**
 * @brief Get current training modulation (LC -> training layer)
 * Returns current NE-based modulation signals for training
 * @param adapter Adapter handle
 * @param modulation Output modulation values
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_get_training_modulation(
    nimcp_lc_adapter_t adapter,
    nimcp_lc_training_modulation_t* modulation
);

/**
 * @brief Register callback for training modulation updates
 * Training layer registers to receive modulation signals
 * @param adapter Adapter handle
 * @param callback Callback function
 * @param user_data User data for callback
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_register_training_callback(
    nimcp_lc_adapter_t adapter,
    nimcp_lc_training_callback_fn callback,
    void* user_data
);

/**
 * @brief Compute attention gain for training features
 * Used by training layer to modulate feature importance
 * @param adapter Adapter handle
 * @param feature_salience Input feature salience
 * @param gain Output attention gain
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_compute_attention_gain(
    nimcp_lc_adapter_t adapter,
    float feature_salience,
    float* gain
);

/**
 * @brief Process novelty detection for training
 * Used when training encounters novel patterns
 * @param adapter Adapter handle
 * @param novelty_score Novelty magnitude [0-1]
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_process_novelty(
    nimcp_lc_adapter_t adapter,
    float novelty_score
);

/**
 * @brief Process training stress (high loss, gradient issues)
 * @param adapter Adapter handle
 * @param stress_level Stress magnitude [0-1]
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_process_training_stress(
    nimcp_lc_adapter_t adapter,
    float stress_level
);

//=============================================================================
// State/Stats API
//=============================================================================

/**
 * @brief Get adapter state
 * @param adapter Adapter handle
 * @param[out] state State output
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_get_state(
    nimcp_lc_adapter_t adapter,
    nimcp_lc_adapter_state_t* state
);

/**
 * @brief Reset adapter statistics
 * @param adapter Adapter handle
 * @return 0 on success
 */
NIMCP_EXPORT int nimcp_lc_adapter_reset_stats(nimcp_lc_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LC_ADAPTER_H */
