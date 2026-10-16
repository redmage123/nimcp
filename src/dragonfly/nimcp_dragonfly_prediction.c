/**
 * @file nimcp_dragonfly_prediction.c
 * @brief Trajectory Prediction and Evasion Detection Implementation
 *
 * WHAT: Implements trajectory prediction with IMM filter and evasion detection
 * WHY:  Dragonflies predict prey trajectory for 95% interception success
 * HOW:  Multiple motion models + Interacting Multiple Model filter
 *
 * @author NIMCP Team
 * @date 2024-12-27
 */

#include "dragonfly/nimcp_dragonfly_prediction.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(dragonfly_prediction)

//=============================================================================
// Constants
//=============================================================================


#define STATE_DIM 9   /* pos(3) + vel(3) + accel(3) */
#define MEAS_DIM  3   /* position only */

//=============================================================================
// Local Helpers
//=============================================================================

static inline uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static inline float vec3_length(const float v[3]) {
    return sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

//=============================================================================
// Single Model Kalman Filter
//=============================================================================

typedef struct {
    float state[STATE_DIM];              /* x,y,z, vx,vy,vz, ax,ay,az */
    float P[STATE_DIM][STATE_DIM];       /* Covariance matrix */
    float Q_base;                        /* Base process noise */
    float R_base;                        /* Base measurement noise */
    prediction_motion_model_t model;     /* Motion model type */
    bool initialized;
    float likelihood;                    /* Last measurement likelihood */
} single_model_filter_t;

static void model_init(single_model_filter_t* m, prediction_motion_model_t model,
                       float q, float r) {
    memset(m, 0, sizeof(*m));
    m->model = model;
    m->Q_base = q;
    m->R_base = r;
    for (int i = 0; i < STATE_DIM; i++) {
        m->P[i][i] = 100.0f;
    }
}

static void model_init_state(single_model_filter_t* m, const float pos[3],
                             const float vel[3]) {
    memcpy(m->state, pos, 3 * sizeof(float));
    if (vel) {
        memcpy(&m->state[3], vel, 3 * sizeof(float));
    }
    m->initialized = true;
}

static void model_predict(single_model_filter_t* m, float dt) {
    if (!m->initialized) return;

    float* x = m->state;

    /* State transition based on model */
    switch (m->model) {
        case PRED_MODEL_CV:
            /* Constant velocity: pos += vel * dt */
            x[0] += x[3] * dt;
            x[1] += x[4] * dt;
            x[2] += x[5] * dt;
            /* Velocity unchanged, accel = 0 */
            x[6] = x[7] = x[8] = 0.0f;
            break;

        case PRED_MODEL_CA:
            /* Constant acceleration */
            x[0] += x[3] * dt + 0.5f * x[6] * dt * dt;
            x[1] += x[4] * dt + 0.5f * x[7] * dt * dt;
            x[2] += x[5] * dt + 0.5f * x[8] * dt * dt;
            x[3] += x[6] * dt;
            x[4] += x[7] * dt;
            x[5] += x[8] * dt;
            break;

        case PRED_MODEL_SINGER:
            /* Singer maneuvering model: accel is correlated noise */
            {
                float alpha = 1.0f / 2.0f;  /* Maneuver time constant */
                float decay = expf(-alpha * dt);
                x[0] += x[3] * dt + 0.5f * x[6] * dt * dt;
                x[1] += x[4] * dt + 0.5f * x[7] * dt * dt;
                x[2] += x[5] * dt + 0.5f * x[8] * dt * dt;
                x[3] += x[6] * dt;
                x[4] += x[7] * dt;
                x[5] += x[8] * dt;
                x[6] *= decay;
                x[7] *= decay;
                x[8] *= decay;
            }
            break;

        case PRED_MODEL_JINK:
            /* Jinking: high process noise on acceleration */
            x[0] += x[3] * dt + 0.5f * x[6] * dt * dt;
            x[1] += x[4] * dt + 0.5f * x[7] * dt * dt;
            x[2] += x[5] * dt + 0.5f * x[8] * dt * dt;
            x[3] += x[6] * dt;
            x[4] += x[7] * dt;
            x[5] += x[8] * dt;
            /* Accel randomly flips - modeled by high Q */
            break;

        case PRED_MODEL_WEAVE:
            /* Weaving: sinusoidal lateral motion */
            {
                /* ~1.5 Hz weave - lateral accel oscillates */
                (void)(2.0f * M_PI * 1.5f);  /* omega reserved for future use */
                x[0] += x[3] * dt;
                x[1] += x[4] * dt;
                x[2] += x[5] * dt;
            }
            break;

        case PRED_MODEL_SPIRAL:
            /* Spiral: constant turn rate */
            {
                float omega = 0.5f;  /* Turn rate rad/s */
                float cos_w = cosf(omega * dt);
                float sin_w = sinf(omega * dt);
                float vx = x[3], vy = x[4];
                x[3] = vx * cos_w - vy * sin_w;
                x[4] = vx * sin_w + vy * cos_w;
                x[0] += x[3] * dt;
                x[1] += x[4] * dt;
                x[2] += x[5] * dt;
            }
            break;
    }

    /* Covariance prediction: P = F*P*F' + Q */
    float q_pos = m->Q_base * dt * dt;
    float q_vel = m->Q_base * dt * 10.0f;
    float q_acc = m->Q_base * 100.0f;

    /* Increase Q for evasive models */
    if (m->model == PRED_MODEL_JINK) {
        q_acc *= 10.0f;
    } else if (m->model == PRED_MODEL_SINGER) {
        q_acc *= 5.0f;
    }

    for (int i = 0; i < 3; i++) {
        m->P[i][i] += q_pos;
        m->P[i+3][i+3] += q_vel;
        m->P[i+6][i+6] += q_acc;
    }
}

static void model_update(single_model_filter_t* m, const float meas[3]) {
    if (!m->initialized) return;

    /* Innovation: y = z - H*x (H extracts position) */
    float y[MEAS_DIM];
    y[0] = meas[0] - m->state[0];
    y[1] = meas[1] - m->state[1];
    y[2] = meas[2] - m->state[2];

    /* Innovation covariance: S = H*P*H' + R */
    float S[MEAS_DIM];
    S[0] = m->P[0][0] + m->R_base;
    S[1] = m->P[1][1] + m->R_base;
    S[2] = m->P[2][2] + m->R_base;

    /* Compute likelihood for IMM */
    float det = S[0] * S[1] * S[2];
    float mahal = y[0]*y[0]/S[0] + y[1]*y[1]/S[1] + y[2]*y[2]/S[2];
    m->likelihood = expf(-0.5f * mahal) / sqrtf(det + 1e-10f);

    /* Kalman gain: K = P*H' * inv(S) */
    float K[STATE_DIM][MEAS_DIM];
    for (int i = 0; i < STATE_DIM; i++) {
        K[i][0] = m->P[i][0] / S[0];
        K[i][1] = m->P[i][1] / S[1];
        K[i][2] = m->P[i][2] / S[2];
    }

    /* State update: x = x + K*y */
    for (int i = 0; i < STATE_DIM; i++) {
        m->state[i] += K[i][0]*y[0] + K[i][1]*y[1] + K[i][2]*y[2];
    }

    /* Covariance update: P = (I - K*H) * P */
    for (int i = 0; i < STATE_DIM; i++) {
        for (int j = 0; j < MEAS_DIM; j++) {
            m->P[i][j] *= (1.0f - K[i][j]);
        }
    }
}

//=============================================================================
// Internal Structures
//=============================================================================

struct dragonfly_predictor_s {
    /* Configuration */
    prediction_config_t config;

    /* IMM filter models */
    single_model_filter_t models[PREDICTOR_MAX_MODELS];
    float model_probs[PREDICTOR_MAX_MODELS];
    uint32_t num_models;

    /* Combined state (IMM output) */
    float combined_state[STATE_DIM];
    float combined_P[STATE_DIM][STATE_DIM];

    /* Evasion state */
    evasion_state_t evasion;
    float accel_history[32][3];
    uint32_t accel_history_idx;
    uint32_t accel_history_count;

    /* Statistics */
    prediction_stats_t stats;

    /* Timing */
    uint64_t last_update_us;
    uint64_t creation_time_us;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// IMM Filter Functions
//=============================================================================

static void imm_mixing(dragonfly_predictor_t* pred) {
    /* Compute mixing probabilities */
    float mixing_probs[PREDICTOR_MAX_MODELS][PREDICTOR_MAX_MODELS];
    float p_trans = pred->config.model_transition_prob;

    for (uint32_t j = 0; j < pred->num_models; j++) {
        float c_j = 0.0f;
        for (uint32_t i = 0; i < pred->num_models; i++) {
            float p_ij = (i == j) ? (1.0f - p_trans) : (p_trans / (pred->num_models - 1));
            c_j += p_ij * pred->model_probs[i];
        }
        for (uint32_t i = 0; i < pred->num_models; i++) {
            float p_ij = (i == j) ? (1.0f - p_trans) : (p_trans / (pred->num_models - 1));
            mixing_probs[i][j] = p_ij * pred->model_probs[i] / (c_j + 1e-10f);
        }
    }

    /* Mix states */
    for (uint32_t j = 0; j < pred->num_models; j++) {
        float mixed_state[STATE_DIM] = {0};
        for (uint32_t i = 0; i < pred->num_models; i++) {
            for (int k = 0; k < STATE_DIM; k++) {
                mixed_state[k] += mixing_probs[i][j] * pred->models[i].state[k];
            }
        }
        memcpy(pred->models[j].state, mixed_state, sizeof(mixed_state));
    }
}

static void imm_combine(dragonfly_predictor_t* pred) {
    /* Combined state = sum of weighted model states */
    memset(pred->combined_state, 0, sizeof(pred->combined_state));
    for (uint32_t i = 0; i < pred->num_models; i++) {
        for (int k = 0; k < STATE_DIM; k++) {
            pred->combined_state[k] += pred->model_probs[i] * pred->models[i].state[k];
        }
    }

    /* Combined covariance (simplified: weighted average of diagonals) */
    memset(pred->combined_P, 0, sizeof(pred->combined_P));
    for (uint32_t i = 0; i < pred->num_models; i++) {
        for (int k = 0; k < STATE_DIM; k++) {
            float diff = pred->models[i].state[k] - pred->combined_state[k];
            pred->combined_P[k][k] += pred->model_probs[i] *
                (pred->models[i].P[k][k] + diff * diff);
        }
    }
}

static void imm_update_probs(dragonfly_predictor_t* pred) {
    /* Update model probabilities based on likelihoods */
    float total = 0.0f;
    for (uint32_t i = 0; i < pred->num_models; i++) {
        pred->model_probs[i] *= pred->models[i].likelihood + 1e-10f;
        total += pred->model_probs[i];
    }

    /* Normalize */
    if (total > 1e-10f) {
        for (uint32_t i = 0; i < pred->num_models; i++) {
            pred->model_probs[i] /= total;
        }
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

prediction_config_t prediction_default_config(void) {
    prediction_config_t config = {
        .enable_imm = false,
        .num_models = 1,
        .models = {PRED_MODEL_CV},
        .model_transition_prob = 0.05f,

        .max_prediction_ms = 500.0f,
        .prediction_steps = 20,
        .process_noise = 0.1f,
        .measurement_noise = 1.0f,

        .jink_accel_threshold = 5.0f,
        .break_decel_threshold = 10.0f,
        .weave_frequency_min = 0.5f,
        .weave_frequency_max = 3.0f,
        .model_switch_threshold = 0.3f,

        .facilitation_width = 0.3f,
        .facilitation_gain = 0.5f
    };
    return config;
}

prediction_config_t prediction_imm_config(void) {
    prediction_config_t config = prediction_default_config();
    config.enable_imm = true;
    config.num_models = 6;
    config.models[0] = PRED_MODEL_CV;
    config.models[1] = PRED_MODEL_CA;
    config.models[2] = PRED_MODEL_SINGER;
    config.models[3] = PRED_MODEL_JINK;
    config.models[4] = PRED_MODEL_WEAVE;
    config.models[5] = PRED_MODEL_SPIRAL;
    return config;
}

bool prediction_validate_config(const prediction_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "prediction_validate_config: config is NULL");
        return false;
    }
    if (config->num_models == 0 || config->num_models > PREDICTOR_MAX_MODELS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prediction_validate_config: config->num_models is zero");
        return false;
    }
    if (config->prediction_steps == 0 || config->prediction_steps > PREDICTOR_MAX_TRAJECTORY) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "prediction_validate_config: config->prediction_steps is zero");
        return false;
    }
    if (config->max_prediction_ms <= 0.0f) {
        return false;
    }
    if (config->process_noise < 0.0f) {
        return false;
    }
    if (config->measurement_noise < 0.0f) {
        return false;
    }
    if (config->facilitation_width <= 0.0f) {
        return false;
    }
    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

dragonfly_predictor_t* dragonfly_predictor_create(const prediction_config_t* config) {
    prediction_config_t cfg = config ? *config : prediction_default_config();

    if (!prediction_validate_config(&cfg)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "dragonfly_predictor_create: invalid configuration");
        return NULL;
    }

    dragonfly_predictor_t* pred = nimcp_calloc(1, sizeof(dragonfly_predictor_t));
    if (!pred) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "dragonfly_predictor_create: failed to allocate predictor");
        return NULL;
    }

    pred->config = cfg;
    pred->num_models = cfg.num_models;
    pred->creation_time_us = get_time_us();

    /* Initialize models */
    for (uint32_t i = 0; i < pred->num_models; i++) {
        model_init(&pred->models[i], cfg.models[i],
                   cfg.process_noise, cfg.measurement_noise);
        pred->model_probs[i] = 1.0f / pred->num_models;
    }

    pred->mutex = nimcp_mutex_create(NULL);
    if (!pred->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "dragonfly_predictor_create: failed to create mutex");
        nimcp_free(pred);
        return NULL;
    }

    return pred;
}

void dragonfly_predictor_destroy(dragonfly_predictor_t* pred) {
    if (!pred) return;
    if (pred->mutex) {
        nimcp_mutex_destroy(pred->mutex);
    }
    nimcp_free(pred);
}

int dragonfly_predictor_reset(dragonfly_predictor_t* pred) {
    if (!pred) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_reset: pred is NULL");
        return -1;
    }

    nimcp_mutex_lock(pred->mutex);

    for (uint32_t i = 0; i < pred->num_models; i++) {
        model_init(&pred->models[i], pred->config.models[i],
                   pred->config.process_noise, pred->config.measurement_noise);
        pred->model_probs[i] = 1.0f / pred->num_models;
    }

    memset(&pred->evasion, 0, sizeof(pred->evasion));
    memset(pred->accel_history, 0, sizeof(pred->accel_history));
    pred->accel_history_idx = 0;
    pred->accel_history_count = 0;

    nimcp_mutex_unlock(pred->mutex);

    return 0;
}

//=============================================================================
// Core Prediction Functions
//=============================================================================

int dragonfly_predictor_update(
    dragonfly_predictor_t* pred,
    const float position[3],
    const float velocity[3],
    float dt
) {
    if (!pred || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_update: required parameter is NULL (pred, position)");
        return -1;
    }
    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_predictor_update: validation failed");
        return -1;
    }

    nimcp_mutex_lock(pred->mutex);

    uint64_t start_us = get_time_us();

    /* Initialize models if needed */
    if (!pred->models[0].initialized) {
        for (uint32_t i = 0; i < pred->num_models; i++) {
            model_init_state(&pred->models[i], position, velocity);
        }
        pred->last_update_us = start_us;
        nimcp_mutex_unlock(pred->mutex);
        return 0;
    }

    /* IMM mixing step */
    if (pred->config.enable_imm && pred->num_models > 1) {
        imm_mixing(pred);
    }

    /* Predict and update each model */
    for (uint32_t i = 0; i < pred->num_models; i++) {
        model_predict(&pred->models[i], dt);
        model_update(&pred->models[i], position);
    }

    /* Update model probabilities */
    if (pred->config.enable_imm && pred->num_models > 1) {
        imm_update_probs(pred);
        imm_combine(pred);
    } else {
        memcpy(pred->combined_state, pred->models[0].state, sizeof(pred->combined_state));
    }

    /* Store acceleration for evasion detection */
    pred->accel_history[pred->accel_history_idx][0] = pred->combined_state[6];
    pred->accel_history[pred->accel_history_idx][1] = pred->combined_state[7];
    pred->accel_history[pred->accel_history_idx][2] = pred->combined_state[8];
    pred->accel_history_idx = (pred->accel_history_idx + 1) % 32;
    if (pred->accel_history_count < 32) pred->accel_history_count++;

    /* Detect evasion */
    float accel_mag = vec3_length(&pred->combined_state[6]);
    if (accel_mag > pred->config.jink_accel_threshold) {
        pred->evasion.maneuver_count++;
        pred->evasion.last_maneuver_us = start_us;
        pred->evasion.maneuver_intensity = nimcp_clampf(
            accel_mag / (pred->config.jink_accel_threshold * 2.0f), 0.0f, 1.0f);
    }

    /* Update evasion classification based on model probabilities */
    if (pred->config.enable_imm) {
        float max_prob = 0.0f;
        uint32_t max_idx = 0;
        for (uint32_t i = 0; i < pred->num_models; i++) {
            pred->evasion.model_probabilities[i] = pred->model_probs[i];
            if (pred->model_probs[i] > max_prob) {
                max_prob = pred->model_probs[i];
                max_idx = i;
            }
        }

        /* Map dominant model to evasion type */
        switch (pred->config.models[max_idx]) {
            case PRED_MODEL_JINK:
                pred->evasion.current_type = EVASION_JINK;
                break;
            case PRED_MODEL_WEAVE:
                pred->evasion.current_type = EVASION_WEAVE;
                break;
            case PRED_MODEL_SPIRAL:
                pred->evasion.current_type = EVASION_SPIRAL;
                break;
            default:
                pred->evasion.current_type = EVASION_NONE;
                break;
        }
    }

    /* Update statistics */
    uint64_t elapsed = get_time_us() - start_us;
    float alpha = 0.1f;
    pred->stats.avg_prediction_time_us =
        (1.0f - alpha) * pred->stats.avg_prediction_time_us + alpha * elapsed;
    pred->stats.predictions_made++;

    pred->last_update_us = start_us;

    nimcp_mutex_unlock(pred->mutex);

    return 0;
}

int dragonfly_predictor_predict(
    dragonfly_predictor_t* pred,
    float lookahead_ms,
    trajectory_prediction_t* prediction
) {
    if (!pred || !prediction) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_predict: required parameter is NULL (pred, prediction)");
        return -1;
    }
    if (lookahead_ms <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_predictor_predict: validation failed");
        return -1;
    }
    if (!prediction->trajectory || prediction->num_points == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_predict: prediction->trajectory is NULL");
        return -1;
    }

    nimcp_mutex_lock(pred->mutex);

    if (!pred->models[0].initialized) {
        nimcp_mutex_unlock(pred->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_predict: pred->models is NULL");
        return -1;
    }

    float dt = lookahead_ms / (float)prediction->num_points / 1000.0f;
    float state[STATE_DIM];
    memcpy(state, pred->combined_state, sizeof(state));

    /* Generate trajectory points */
    for (uint32_t i = 0; i < prediction->num_points; i++) {
        predicted_state_t* pt = &prediction->trajectory[i];

        /* Propagate state forward */
        state[0] += state[3] * dt + 0.5f * state[6] * dt * dt;
        state[1] += state[4] * dt + 0.5f * state[7] * dt * dt;
        state[2] += state[5] * dt + 0.5f * state[8] * dt * dt;
        state[3] += state[6] * dt;
        state[4] += state[7] * dt;
        state[5] += state[8] * dt;

        memcpy(pt->position, state, 3 * sizeof(float));
        memcpy(pt->velocity, &state[3], 3 * sizeof(float));
        memcpy(pt->acceleration, &state[6], 3 * sizeof(float));

        pt->time_offset_ms = (i + 1) * (lookahead_ms / prediction->num_points);

        /* Confidence decays with time and evasion */
        float time_decay = expf(-pt->time_offset_ms / pred->config.max_prediction_ms);
        float evasion_penalty = 1.0f - 0.5f * pred->evasion.maneuver_intensity;
        pt->confidence = time_decay * evasion_penalty;

        /* Store covariance (diagonal only) */
        for (int k = 0; k < 9; k++) {
            pt->covariance[k] = pred->combined_P[k][k] * (1.0f + pt->time_offset_ms / 100.0f);
        }
    }

    /* Copy evasion state */
    prediction->evasion = pred->evasion;
    prediction->prediction_horizon_ms = lookahead_ms;
    prediction->timestamp_us = get_time_us();

    /* Find optimal intercept point (highest confidence) */
    float best_conf = 0.0f;
    for (uint32_t i = 0; i < prediction->num_points; i++) {
        if (prediction->trajectory[i].confidence > best_conf) {
            best_conf = prediction->trajectory[i].confidence;
            memcpy(prediction->optimal_intercept_point,
                   prediction->trajectory[i].position, 3 * sizeof(float));
            prediction->optimal_intercept_time_ms = prediction->trajectory[i].time_offset_ms;
        }
    }

    nimcp_mutex_unlock(pred->mutex);

    return 0;
}

int dragonfly_predictor_get_state_at(
    dragonfly_predictor_t* pred,
    float time_offset_ms,
    predicted_state_t* state
) {
    if (!pred || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_get_state_at: required parameter is NULL (pred, state)");
        return -1;
    }
    if (time_offset_ms < 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_predictor_get_state_at: validation failed");
        return -1;
    }

    nimcp_mutex_lock(pred->mutex);

    if (!pred->models[0].initialized) {
        nimcp_mutex_unlock(pred->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_get_state_at: pred->models is NULL");
        return -1;
    }

    float dt = time_offset_ms / 1000.0f;
    float s[STATE_DIM];
    memcpy(s, pred->combined_state, sizeof(s));

    /* Simple forward propagation */
    s[0] += s[3] * dt + 0.5f * s[6] * dt * dt;
    s[1] += s[4] * dt + 0.5f * s[7] * dt * dt;
    s[2] += s[5] * dt + 0.5f * s[8] * dt * dt;
    s[3] += s[6] * dt;
    s[4] += s[7] * dt;
    s[5] += s[8] * dt;

    memcpy(state->position, s, 3 * sizeof(float));
    memcpy(state->velocity, &s[3], 3 * sizeof(float));
    memcpy(state->acceleration, &s[6], 3 * sizeof(float));
    state->time_offset_ms = time_offset_ms;
    state->confidence = expf(-time_offset_ms / pred->config.max_prediction_ms);

    nimcp_mutex_unlock(pred->mutex);

    return 0;
}

//=============================================================================
// Evasion Detection Functions
//=============================================================================

int dragonfly_predictor_get_evasion(
    const dragonfly_predictor_t* pred,
    evasion_state_t* evasion
) {
    if (!pred || !evasion) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_get_evasion: required parameter is NULL (pred, evasion)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)pred->mutex);
    *evasion = pred->evasion;
    nimcp_mutex_unlock((nimcp_mutex_t*)pred->mutex);

    return 0;
}

evasion_type_t dragonfly_predictor_detect_evasion(
    dragonfly_predictor_t* pred,
    const float observed_accel[3]
) {
    if (!pred || !observed_accel) return EVASION_NONE;

    float mag = vec3_length(observed_accel);

    /* Check for jink (sudden direction change) */
    if (mag > pred->config.jink_accel_threshold) {
        return EVASION_JINK;
    }

    /* Check for break (strong deceleration along velocity) */
    if (pred->models[0].initialized) {
        float* vel = &pred->combined_state[3];
        float speed = vec3_length(vel);
        if (speed > 0.1f) {
            float decel = -(observed_accel[0] * vel[0] +
                           observed_accel[1] * vel[1] +
                           observed_accel[2] * vel[2]) / speed;
            if (decel > pred->config.break_decel_threshold) {
                return EVASION_BREAK;
            }
        }
    }

    /* TODO: Frequency analysis for weave detection */

    return EVASION_NONE;
}

float dragonfly_predictor_get_evasion_intensity(const dragonfly_predictor_t* pred) {
    if (!pred) return 0.0f;
    return pred->evasion.maneuver_intensity;
}

//=============================================================================
// Forward and Inverse Models
//=============================================================================

int dragonfly_forward_model(
    const dragonfly_predictor_t* pred,
    const float current_state[9],
    const float action[3],
    float dt,
    float predicted_state[9]
) {
    if (!current_state || !action || !predicted_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_forward_model: required parameter is NULL (current_state, action, predicted_state)");
        return -1;
    }
    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_forward_model: validation failed");
        return -1;
    }

    /* pos' = pos + vel*dt + 0.5*accel*dt^2 + 0.5*action*dt^2 */
    predicted_state[0] = current_state[0] + current_state[3] * dt +
                         0.5f * (current_state[6] + action[0]) * dt * dt;
    predicted_state[1] = current_state[1] + current_state[4] * dt +
                         0.5f * (current_state[7] + action[1]) * dt * dt;
    predicted_state[2] = current_state[2] + current_state[5] * dt +
                         0.5f * (current_state[8] + action[2]) * dt * dt;

    /* vel' = vel + accel*dt + action*dt */
    predicted_state[3] = current_state[3] + (current_state[6] + action[0]) * dt;
    predicted_state[4] = current_state[4] + (current_state[7] + action[1]) * dt;
    predicted_state[5] = current_state[5] + (current_state[8] + action[2]) * dt;

    /* accel' = action (action becomes new acceleration) */
    predicted_state[6] = action[0];
    predicted_state[7] = action[1];
    predicted_state[8] = action[2];

    return 0;
}

int dragonfly_inverse_model(
    const dragonfly_predictor_t* pred,
    const float current_state[9],
    const float desired_state[9],
    float dt,
    float required_action[3]
) {
    if (!current_state || !desired_state || !required_action) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_inverse_model: required parameter is NULL (current_state, desired_state, required_action)");
        return -1;
    }
    if (dt <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_inverse_model: validation failed");
        return -1;
    }

    /* Solve for action that achieves desired velocity */
    /* vel' = vel + (accel + action) * dt */
    /* action = (vel' - vel) / dt - accel */
    required_action[0] = (desired_state[3] - current_state[3]) / dt - current_state[6];
    required_action[1] = (desired_state[4] - current_state[4]) / dt - current_state[7];
    required_action[2] = (desired_state[5] - current_state[5]) / dt - current_state[8];

    return 0;
}

//=============================================================================
// IMM Filter Functions
//=============================================================================

int dragonfly_predictor_get_model_probabilities(
    const dragonfly_predictor_t* pred,
    float* probabilities,
    uint32_t num_models
) {
    if (!pred || !probabilities) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_get_model_probabilities: required parameter is NULL (pred, probabilities)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)pred->mutex);

    uint32_t n = num_models < pred->num_models ? num_models : pred->num_models;
    memcpy(probabilities, pred->model_probs, n * sizeof(float));

    nimcp_mutex_unlock((nimcp_mutex_t*)pred->mutex);

    return 0;
}

prediction_motion_model_t dragonfly_predictor_get_dominant_model(
    const dragonfly_predictor_t* pred
) {
    if (!pred) return PRED_MODEL_CV;

    float max_prob = 0.0f;
    uint32_t max_idx = 0;

    for (uint32_t i = 0; i < pred->num_models; i++) {
        if (pred->model_probs[i] > max_prob) {
            max_prob = pred->model_probs[i];
            max_idx = i;
        }
    }

    return pred->config.models[max_idx];
}

//=============================================================================
// Facilitation Functions
//=============================================================================

float dragonfly_predictor_get_facilitation_gain(
    const dragonfly_predictor_t* pred,
    float direction
) {
    if (!pred) return 1.0f;
    if (!pred->models[0].initialized) return 1.0f;

    float pred_dir = dragonfly_predictor_get_predicted_direction(pred);
    float diff = direction - pred_dir;

    /* Normalize to [-pi, pi] */
    while (diff > M_PI) diff -= 2.0f * M_PI;
    while (diff < -M_PI) diff += 2.0f * M_PI;

    /* Gaussian facilitation centered on predicted direction */
    float width = pred->config.facilitation_width;
    float gain_boost = pred->config.facilitation_gain;
    float facilitation = gain_boost * expf(-diff * diff / (2.0f * width * width));

    return 1.0f + facilitation;
}

float dragonfly_predictor_get_predicted_direction(const dragonfly_predictor_t* pred) {
    if (!pred) return 0.0f;
    if (!pred->models[0].initialized) return 0.0f;

    /* Direction from velocity */
    float vx = pred->combined_state[3];
    float vy = pred->combined_state[4];

    return atan2f(vy, vx);
}

//=============================================================================
// Statistics and Configuration
//=============================================================================

int dragonfly_predictor_get_stats(
    const dragonfly_predictor_t* pred,
    prediction_stats_t* stats
) {
    if (!pred || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_get_stats: required parameter is NULL (pred, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)pred->mutex);
    *stats = pred->stats;
    nimcp_mutex_unlock((nimcp_mutex_t*)pred->mutex);

    return 0;
}

int dragonfly_predictor_reset_stats(dragonfly_predictor_t* pred) {
    if (!pred) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_reset_stats: pred is NULL");
        return -1;
    }

    nimcp_mutex_lock(pred->mutex);
    memset(&pred->stats, 0, sizeof(pred->stats));
    nimcp_mutex_unlock(pred->mutex);

    return 0;
}

int dragonfly_predictor_set_config(
    dragonfly_predictor_t* pred,
    const prediction_config_t* config
) {
    if (!pred || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_set_config: required parameter is NULL (pred, config)");
        return -1;
    }
    if (!prediction_validate_config(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_predictor_set_config: prediction_validate_config is NULL");
        return -1;
    }

    nimcp_mutex_lock(pred->mutex);
    pred->config = *config;
    nimcp_mutex_unlock(pred->mutex);

    return 0;
}

int dragonfly_predictor_get_config(
    const dragonfly_predictor_t* pred,
    prediction_config_t* config
) {
    if (!pred || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dragonfly_predictor_get_config: required parameter is NULL (pred, config)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)pred->mutex);
    *config = pred->config;
    nimcp_mutex_unlock((nimcp_mutex_t*)pred->mutex);

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* dragonfly_evasion_name(evasion_type_t type) {
    switch (type) {
        case EVASION_NONE:     return "NONE";
        case EVASION_JINK:     return "JINK";
        case EVASION_BREAK:    return "BREAK";
        case EVASION_WEAVE:    return "WEAVE";
        case EVASION_SPIRAL:   return "SPIRAL";
        case EVASION_COMBINED: return "COMBINED";
        default:               return "UNKNOWN";
    }
}

const char* dragonfly_model_name(prediction_motion_model_t model) {
    switch (model) {
        case PRED_MODEL_CV:     return "CONSTANT_VELOCITY";
        case PRED_MODEL_CA:     return "CONSTANT_ACCEL";
        case PRED_MODEL_SINGER: return "SINGER";
        case PRED_MODEL_JINK:   return "JINK";
        case PRED_MODEL_WEAVE:  return "WEAVE";
        case PRED_MODEL_SPIRAL: return "SPIRAL";
        default:                  return "UNKNOWN";
    }
}

predicted_state_t* dragonfly_trajectory_alloc(uint32_t num_points) {
    if (num_points == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dragonfly_trajectory_alloc: num_points is zero");
        return NULL;
    }
    return nimcp_calloc(num_points, sizeof(predicted_state_t));
}

void dragonfly_trajectory_free(predicted_state_t* trajectory) {
    nimcp_free(trajectory);
}
