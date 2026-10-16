/**
 * @file nimcp_habenula_adapter.h
 * @brief Habenula adapter with bidirectional training layer integration
 *
 * The Habenula adapter provides:
 * - Bio-async messaging integration
 * - Bidirectional training layer communication
 * - VTA and Raphe coordination
 * - Negative reinforcement signal generation
 * - Depression/helplessness state management
 */

#ifndef NIMCP_HABENULA_ADAPTER_H
#define NIMCP_HABENULA_ADAPTER_H

#include "nimcp_habenula.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct nimcp_habenula_adapter_impl;

/*=============================================================================
 * Types
 *===========================================================================*/

/**
 * Opaque adapter handle
 */
typedef struct nimcp_habenula_adapter_impl* nimcp_habenula_adapter_t;

/**
 * Training event types that affect Habenula
 */
typedef enum {
    HABENULA_TRAIN_EVENT_LOSS,          /* Training loss reported */
    HABENULA_TRAIN_EVENT_GRADIENT,      /* Gradient update */
    HABENULA_TRAIN_EVENT_FAILURE,       /* Learning failure */
    HABENULA_TRAIN_EVENT_TIMEOUT,       /* Task timeout */
    HABENULA_TRAIN_EVENT_REWARD,        /* Positive reward */
    HABENULA_TRAIN_EVENT_PUNISHMENT,    /* Negative outcome */
    HABENULA_TRAIN_EVENT_EPOCH_END,     /* Epoch completed */
    HABENULA_TRAIN_EVENT_PLATEAU        /* Learning plateau detected */
} nimcp_habenula_train_event_t;

/**
 * Training event data
 */
typedef struct {
    nimcp_habenula_train_event_t type;
    float value;                /* Associated value (loss, reward, etc.) */
    float expected;             /* Expected outcome */
    float timestamp;            /* Event timestamp */
} nimcp_habenula_train_event_data_t;

/**
 * Training modulation signals (Habenula -> Training layer)
 */
typedef struct {
    float lr_reduction_factor;      /* Reduce LR during disappointment */
    float exploration_penalty;      /* Penalty for risky exploration */
    float negative_weight_factor;   /* Weight for negative examples */
    float patience_reduction;       /* Reduce patience for stopping */
    bool suggest_early_stop;        /* Suggest stopping training */
    bool suggest_checkpoint;        /* Suggest saving checkpoint */
    float avoidance_signal;         /* Signal to avoid current approach */
} nimcp_habenula_training_modulation_t;

/**
 * Message callback signature
 */
typedef void (*nimcp_habenula_msg_callback_t)(void* user_data,
                                               const char* topic,
                                               const void* data,
                                               size_t size);

/**
 * Training modulation callback signature
 * Called when habenula wants to communicate with training layer
 */
typedef void (*nimcp_habenula_training_callback_t)(
    void* user_data,
    const nimcp_habenula_training_modulation_t* modulation);

/**
 * Adapter configuration
 */
typedef struct {
    nimcp_habenula_config_t habenula_config;
    bool enable_training_integration;
    bool enable_vta_coordination;
    bool enable_raphe_coordination;
    float training_update_interval;     /* How often to update training layer */
    float disappointment_lr_scale;      /* How much disappointment affects LR */
    float failure_penalty_scale;        /* How much failures penalize */
} nimcp_habenula_adapter_config_t;

/**
 * Adapter state for external inspection
 */
typedef struct {
    float firing_rate;
    float disappointment;
    float aversion;
    float vta_inhibition;
    float raphe_modulation;
    float helplessness;
    bool is_depressed;
    nimcp_habenula_mode_t mode;
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t training_events;
} nimcp_habenula_adapter_state_t;

/*=============================================================================
 * Configuration API
 *===========================================================================*/

/**
 * Get default adapter configuration
 */
void nimcp_habenula_adapter_default_config(nimcp_habenula_adapter_config_t* config);

/*=============================================================================
 * Lifecycle API
 *===========================================================================*/

/**
 * Create habenula adapter
 * @param config Configuration (NULL for defaults)
 * @return Adapter handle or NULL on failure
 */
nimcp_habenula_adapter_t nimcp_habenula_adapter_create(
    const nimcp_habenula_adapter_config_t* config);

/**
 * Destroy habenula adapter
 */
void nimcp_habenula_adapter_destroy(nimcp_habenula_adapter_t adapter);

/**
 * Disconnect adapter from messaging systems
 */
int nimcp_habenula_adapter_disconnect(nimcp_habenula_adapter_t adapter);

/*=============================================================================
 * Access API
 *===========================================================================*/

/**
 * Get underlying habenula system
 */
nimcp_habenula_system_t* nimcp_habenula_adapter_get_habenula(
    nimcp_habenula_adapter_t adapter);

/*=============================================================================
 * Messaging API
 *===========================================================================*/

/**
 * Send message to other brain regions
 */
int nimcp_habenula_adapter_send_message(nimcp_habenula_adapter_t adapter,
                                         const char* topic,
                                         const void* data,
                                         size_t size);

/**
 * Process incoming messages
 */
int nimcp_habenula_adapter_process_messages(nimcp_habenula_adapter_t adapter);

/**
 * Register message callback
 */
int nimcp_habenula_adapter_register_callback(nimcp_habenula_adapter_t adapter,
                                              const char* topic,
                                              nimcp_habenula_msg_callback_t callback,
                                              void* user_data);

/*=============================================================================
 * Update API
 *===========================================================================*/

/**
 * Update adapter state
 * @param adapter Adapter handle
 * @param dt_ms Time step in milliseconds
 */
int nimcp_habenula_adapter_update(nimcp_habenula_adapter_t adapter, float dt_ms);

/*=============================================================================
 * Training Layer Integration API (Bidirectional)
 *===========================================================================*/

/**
 * Connect to training layer
 */
int nimcp_habenula_adapter_connect_training(nimcp_habenula_adapter_t adapter,
                                             void* training_handle);

/**
 * Process training event (Training -> Habenula)
 * @param adapter Adapter handle
 * @param event Event data
 */
int nimcp_habenula_adapter_on_training_event(
    nimcp_habenula_adapter_t adapter,
    const nimcp_habenula_train_event_data_t* event);

/**
 * Get training modulation (Habenula -> Training)
 * @param adapter Adapter handle
 * @param modulation Output modulation values
 */
int nimcp_habenula_adapter_get_training_modulation(
    nimcp_habenula_adapter_t adapter,
    nimcp_habenula_training_modulation_t* modulation);

/**
 * Register training modulation callback
 * Called when habenula computes new modulation values
 */
int nimcp_habenula_adapter_register_training_callback(
    nimcp_habenula_adapter_t adapter,
    nimcp_habenula_training_callback_t callback,
    void* user_data);

/*=============================================================================
 * Reward Processing API
 *===========================================================================*/

/**
 * Process reward outcome for training
 */
int nimcp_habenula_adapter_process_reward_outcome(
    nimcp_habenula_adapter_t adapter,
    float expected,
    float received);

/**
 * Process punishment/negative outcome
 */
int nimcp_habenula_adapter_process_punishment(
    nimcp_habenula_adapter_t adapter,
    float intensity);

/**
 * Compute negative reinforcement signal for training
 */
int nimcp_habenula_adapter_compute_negative_reinforcement(
    nimcp_habenula_adapter_t adapter,
    float* negative_signal);

/*=============================================================================
 * VTA Coordination API
 *===========================================================================*/

/**
 * Get VTA inhibition output
 * This signal is sent to VTA to suppress dopamine during negative outcomes
 */
int nimcp_habenula_adapter_get_vta_output(
    nimcp_habenula_adapter_t adapter,
    float* inhibition);

/**
 * Apply VTA dopamine feedback
 * DA level from VTA affects habenula activity
 */
int nimcp_habenula_adapter_apply_vta_input(
    nimcp_habenula_adapter_t adapter,
    float da_level);

/*=============================================================================
 * Raphe Coordination API
 *===========================================================================*/

/**
 * Get Raphe modulation output
 */
int nimcp_habenula_adapter_get_raphe_output(
    nimcp_habenula_adapter_t adapter,
    float* modulation);

/**
 * Apply Raphe serotonin feedback
 */
int nimcp_habenula_adapter_apply_raphe_input(
    nimcp_habenula_adapter_t adapter,
    float ht_level);

/*=============================================================================
 * Depression/Helplessness API
 *===========================================================================*/

/**
 * Check if system suggests stopping (learned helplessness)
 */
int nimcp_habenula_adapter_should_stop(
    nimcp_habenula_adapter_t adapter,
    bool* should_stop);

/**
 * Get depression/helplessness state
 */
int nimcp_habenula_adapter_get_depression_state(
    nimcp_habenula_adapter_t adapter,
    float* helplessness,
    bool* is_depressed);

/**
 * Record failure for helplessness model
 */
int nimcp_habenula_adapter_record_failure(nimcp_habenula_adapter_t adapter);

/**
 * Record success (recovery from helplessness)
 */
int nimcp_habenula_adapter_record_success(nimcp_habenula_adapter_t adapter);

/*=============================================================================
 * State API
 *===========================================================================*/

/**
 * Get adapter state
 */
int nimcp_habenula_adapter_get_state(nimcp_habenula_adapter_t adapter,
                                      nimcp_habenula_adapter_state_t* state);

/**
 * Reset statistics
 */
int nimcp_habenula_adapter_reset_stats(nimcp_habenula_adapter_t adapter);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HABENULA_ADAPTER_H */
