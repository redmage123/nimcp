/**
 * @file nimcp_world_model_trainer.h
 * @brief Predictive World Model Trainer — trains brain to predict next state
 *        from (current state + action). Prediction error = understanding gap.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#ifndef NIMCP_WORLD_MODEL_TRAINER_H
#define NIMCP_WORLD_MODEL_TRAINER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NIMCP_WMT_MAX_STATE_DIM   256
#define NIMCP_WMT_MAX_ACTION_DIM   32
#define NIMCP_WMT_HISTORY_CAPACITY 100

/* ============================================================================
 * Configuration
 * ============================================================================ */

typedef struct {
    uint32_t prediction_horizon;   /**< Steps ahead to predict (default 5) */
    float    prediction_lr;        /**< Learning rate (default 0.001) */
    float    error_ema_alpha;      /**< EMA smoothing for error (default 0.05) */
} nimcp_wmt_config_t;

/* ============================================================================
 * Opaque Handle
 * ============================================================================ */

typedef struct nimcp_world_model_trainer nimcp_world_model_trainer_t;

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * @brief Create a world model trainer.
 * @param config Configuration. NULL uses defaults.
 * @return Handle, or NULL on failure.
 */
nimcp_world_model_trainer_t* nimcp_wmt_create(const nimcp_wmt_config_t* config);

/**
 * @brief Destroy a world model trainer. NULL-safe.
 */
void nimcp_wmt_destroy(nimcp_world_model_trainer_t* wmt);

/**
 * @brief Return default configuration.
 */
nimcp_wmt_config_t nimcp_wmt_config_default(void);

/* ============================================================================
 * Recording & Training
 * ============================================================================ */

/**
 * @brief Record a state transition for training.
 * @param wmt       Handle.
 * @param state     Current state vector.
 * @param state_dim Dimension of state (clamped to MAX_STATE_DIM).
 * @param action    Action vector.
 * @param action_dim Dimension of action (clamped to MAX_ACTION_DIM).
 * @param next_state Resulting next state vector.
 * @return 0 on success, -1 on failure.
 */
int nimcp_wmt_record_transition(nimcp_world_model_trainer_t* wmt,
    const float* state, uint32_t state_dim,
    const float* action, uint32_t action_dim,
    const float* next_state);

/**
 * @brief Train the brain's predictor using stored transitions.
 *
 * Uses nimcp_brain_learn_vector with state+action concatenated as features
 * and next_state as target, label="world_model_prediction".
 *
 * @param wmt   Handle.
 * @param brain Brain handle (nimcp_brain_t).
 * @return Prediction error (>= 0), or -1.0f on failure.
 */
float nimcp_wmt_train_predictor(nimcp_world_model_trainer_t* wmt, void* brain);

/**
 * @brief Predict next state from current (state, action) using brain inference.
 * @param wmt            Handle.
 * @param brain          Brain handle.
 * @param state          Current state.
 * @param state_dim      State dimension.
 * @param action         Action vector.
 * @param action_dim     Action dimension.
 * @param predicted_next Output buffer for predicted next state.
 * @param max_dim        Size of output buffer.
 * @return Number of dimensions written, or -1 on failure.
 */
int nimcp_wmt_predict(nimcp_world_model_trainer_t* wmt, void* brain,
    const float* state, uint32_t state_dim,
    const float* action, uint32_t action_dim,
    float* predicted_next, uint32_t max_dim);

/**
 * @brief Get current EMA prediction error.
 * @return EMA error, or 0.0f if no training has occurred.
 */
float nimcp_wmt_get_prediction_error(const nimcp_world_model_trainer_t* wmt);

/**
 * @brief Connect a world prior (physics + chemistry + biology) to the trainer.
 *
 * When connected, the trainer adds a physics/chemistry/biology loss term to
 * the prediction error, penalizing predictions that violate natural laws.
 *
 * @param wmt         Handle.
 * @param world_prior World prior handle (world_prior_t*), or NULL to disconnect.
 */
void nimcp_wmt_set_world_prior(nimcp_world_model_trainer_t* wmt, void* world_prior);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WORLD_MODEL_TRAINER_H */
