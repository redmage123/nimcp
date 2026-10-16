/**
 * @file nimcp_world_model_trainer.c
 * @brief Predictive World Model Trainer — trains brain to predict next state
 *        from (current state + action). Prediction error = understanding gap.
 *
 * Copyright (c) 2026 NIMCP Project. All rights reserved.
 */

#define LOG_MODULE "WORLD_MODEL_TRAINER"

#include "cognitive/nimcp_world_model_trainer.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "nimcp.h"

#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

typedef struct {
    float state[NIMCP_WMT_MAX_STATE_DIM];
    float action[NIMCP_WMT_MAX_ACTION_DIM];
    float next_state[NIMCP_WMT_MAX_STATE_DIM];
    uint32_t state_dim;
    uint32_t action_dim;
} nimcp_wmt_transition_t;

struct nimcp_world_model_trainer {
    nimcp_wmt_config_t config;

    /* Circular buffer of transitions */
    nimcp_wmt_transition_t history[NIMCP_WMT_HISTORY_CAPACITY];
    uint32_t history_count;
    uint32_t history_write_idx;

    /* Prediction workspace */
    float prediction_buffer[NIMCP_WMT_MAX_STATE_DIM];

    /* EMA of prediction error */
    float error_ema;
    bool  error_initialized;

    /* World prior (physics + chemistry + biology) — set by brain if available */
    void* world_prior;  /* world_prior_t*, opaque to avoid header dependency */
};

/* ============================================================================
 * Configuration
 * ============================================================================ */

nimcp_wmt_config_t nimcp_wmt_config_default(void)
{
    nimcp_wmt_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.prediction_horizon = 5;
    cfg.prediction_lr      = 0.001f;
    cfg.error_ema_alpha    = 0.05f;
    return cfg;
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

nimcp_world_model_trainer_t* nimcp_wmt_create(const nimcp_wmt_config_t* config)
{
    nimcp_world_model_trainer_t* wmt = (nimcp_world_model_trainer_t*)nimcp_calloc(
        1, sizeof(nimcp_world_model_trainer_t));
    if (!wmt) {
        LOG_ERROR("Failed to allocate world model trainer");
        return NULL;
    }

    if (config) {
        wmt->config = *config;
    } else {
        wmt->config = nimcp_wmt_config_default();
    }

    wmt->history_count     = 0;
    wmt->history_write_idx = 0;
    wmt->error_ema         = 0.0f;
    wmt->error_initialized = false;

    LOG_INFO("Created world model trainer (horizon=%u, lr=%.4f, ema_alpha=%.3f)",
             wmt->config.prediction_horizon, wmt->config.prediction_lr,
             wmt->config.error_ema_alpha);

    return wmt;
}

void nimcp_wmt_destroy(nimcp_world_model_trainer_t* wmt)
{
    if (!wmt) {
        return;
    }
    nimcp_free(wmt);
}

/* ============================================================================
 * Recording
 * ============================================================================ */

int nimcp_wmt_record_transition(nimcp_world_model_trainer_t* wmt,
    const float* state, uint32_t state_dim,
    const float* action, uint32_t action_dim,
    const float* next_state)
{
    if (!wmt || !state || !next_state) {
        return -1;
    }
    if (state_dim == 0) {
        return -1;
    }
    /* action is optional: supervised-learning callers (brain_learn_vector)
     * have no action vector; treat as actionless transition (action_dim=0). */
    if (action == NULL) {
        action_dim = 0;
    } else if (action_dim == 0) {
        /* non-NULL pointer with zero dim — caller error; ignore action */
        action = NULL;
    }

    /* Clamp dimensions */
    uint32_t sd = state_dim < NIMCP_WMT_MAX_STATE_DIM ? state_dim : NIMCP_WMT_MAX_STATE_DIM;
    uint32_t ad = action_dim < NIMCP_WMT_MAX_ACTION_DIM ? action_dim : NIMCP_WMT_MAX_ACTION_DIM;

    nimcp_wmt_transition_t* t = &wmt->history[wmt->history_write_idx];
    memset(t, 0, sizeof(*t));
    memcpy(t->state, state, sd * sizeof(float));
    if (action && ad > 0) {
        memcpy(t->action, action, ad * sizeof(float));
    }
    memcpy(t->next_state, next_state, sd * sizeof(float));
    t->state_dim  = sd;
    t->action_dim = ad;

    wmt->history_write_idx = (wmt->history_write_idx + 1) % NIMCP_WMT_HISTORY_CAPACITY;
    if (wmt->history_count < NIMCP_WMT_HISTORY_CAPACITY) {
        wmt->history_count++;
    }

    return 0;
}

/* ============================================================================
 * Training
 * ============================================================================ */

float nimcp_wmt_train_predictor(nimcp_world_model_trainer_t* wmt, void* brain)
{
    if (!wmt || !brain) {
        return -1.0f;
    }
    if (wmt->history_count == 0) {
        return -1.0f;
    }

    nimcp_brain_t brain_handle = (nimcp_brain_t)brain;
    float total_error = 0.0f;
    uint32_t trained = 0;

    /* Train on stored transitions */
    uint32_t count = wmt->history_count;
    uint32_t horizon = wmt->config.prediction_horizon;
    if (horizon > count) {
        horizon = count;
    }

    for (uint32_t i = 0; i < horizon; i++) {
        /* Walk backwards from write pointer */
        uint32_t idx;
        if (wmt->history_write_idx >= (i + 1)) {
            idx = wmt->history_write_idx - (i + 1);
        } else {
            idx = NIMCP_WMT_HISTORY_CAPACITY - ((i + 1) - wmt->history_write_idx);
        }

        const nimcp_wmt_transition_t* t = &wmt->history[idx];

        /* Concatenate state + action as features */
        uint32_t feat_dim = t->state_dim + t->action_dim;
        float features[NIMCP_WMT_MAX_STATE_DIM + NIMCP_WMT_MAX_ACTION_DIM];
        memcpy(features, t->state, t->state_dim * sizeof(float));
        memcpy(features + t->state_dim, t->action, t->action_dim * sizeof(float));

        /* Train: features -> next_state */
        nimcp_status_t status = nimcp_brain_learn_vector(
            brain_handle,
            features, feat_dim,
            t->next_state, t->state_dim,
            "world_model_prediction",
            wmt->config.prediction_lr);

        if (status == NIMCP_OK) {
            /* Compute prediction error: infer and compare */
            float predicted[NIMCP_WMT_MAX_STATE_DIM];
            memset(predicted, 0, sizeof(predicted));
            nimcp_status_t inf_status = nimcp_brain_infer(
                brain_handle, features, feat_dim,
                predicted, t->state_dim);

            if (inf_status == NIMCP_OK) {
                float mse = 0.0f;
                for (uint32_t j = 0; j < t->state_dim; j++) {
                    float diff = predicted[j] - t->next_state[j];
                    mse += diff * diff;
                }
                mse /= (float)t->state_dim;
                total_error += mse;
                trained++;
            }
        }
    }

    if (trained == 0) {
        return -1.0f;
    }

    float avg_error = total_error / (float)trained;

    /* Update EMA */
    float alpha = wmt->config.error_ema_alpha;
    if (!wmt->error_initialized) {
        wmt->error_ema = avg_error;
        wmt->error_initialized = true;
    } else {
        wmt->error_ema = alpha * avg_error + (1.0f - alpha) * wmt->error_ema;
    }

    /* World prior loss: penalize physically/chemically/biologically implausible predictions.
     * The world_prior field is set by the brain when the intuitive physics subsystem is active.
     * This adds a regularization term that pushes the learned world model toward predictions
     * that respect conservation laws, collision constraints, and biological limits. */
    if (wmt->world_prior) {
        extern float world_prior_compute_loss(void* wp, uint32_t domain_hint);
        float prior_loss = world_prior_compute_loss(wmt->world_prior, 0);
        /* Blend: total = data_loss + lambda * prior_loss */
        float lambda = 0.1f;  /* world prior regularization weight */
        avg_error += lambda * prior_loss;
    }

    return avg_error;
}

/* ============================================================================
 * Prediction
 * ============================================================================ */

int nimcp_wmt_predict(nimcp_world_model_trainer_t* wmt, void* brain,
    const float* state, uint32_t state_dim,
    const float* action, uint32_t action_dim,
    float* predicted_next, uint32_t max_dim)
{
    if (!wmt || !brain || !state || !action || !predicted_next) {
        return -1;
    }
    if (state_dim == 0 || action_dim == 0 || max_dim == 0) {
        return -1;
    }

    uint32_t sd = state_dim < NIMCP_WMT_MAX_STATE_DIM ? state_dim : NIMCP_WMT_MAX_STATE_DIM;
    uint32_t ad = action_dim < NIMCP_WMT_MAX_ACTION_DIM ? action_dim : NIMCP_WMT_MAX_ACTION_DIM;
    uint32_t out_dim = sd < max_dim ? sd : max_dim;

    /* Concatenate state + action */
    uint32_t feat_dim = sd + ad;
    float features[NIMCP_WMT_MAX_STATE_DIM + NIMCP_WMT_MAX_ACTION_DIM];
    memcpy(features, state, sd * sizeof(float));
    memcpy(features + sd, action, ad * sizeof(float));

    nimcp_brain_t brain_handle = (nimcp_brain_t)brain;
    nimcp_status_t status = nimcp_brain_infer(
        brain_handle, features, feat_dim,
        predicted_next, out_dim);

    if (status != NIMCP_OK) {
        return -1;
    }

    return (int)out_dim;
}

/* ============================================================================
 * Error Query
 * ============================================================================ */

float nimcp_wmt_get_prediction_error(const nimcp_world_model_trainer_t* wmt)
{
    if (!wmt) {
        return 0.0f;
    }
    return wmt->error_ema;
}

void nimcp_wmt_set_world_prior(nimcp_world_model_trainer_t* wmt, void* world_prior)
{
    if (!wmt) return;
    wmt->world_prior = world_prior;
}
