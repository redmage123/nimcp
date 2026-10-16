/**
 * @file nimcp_lgss_training_guard.c
 * @brief Implementation of LGSS Training Guard
 * @version 1.0.0
 * @date 2026-01-16
 *
 * Implements the training guard that constrains gradient-based learning updates
 * and detects dangerous training patterns like reward hacking and goal drift.
 */

#include "security/lgss/learning/nimcp_lgss_training_guard.h"
#include "utils/validation/nimcp_common.h"
#include "utils/error/nimcp_error_codes.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "constants/nimcp_buffer_constants.h"

BRIDGE_BOILERPLATE_MESH_ONLY(lgss_training_guard, MESH_ADAPTER_CATEGORY_SECURITY)


/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/** Circular buffer for reward history */
typedef struct {
    float* values;
    uint64_t* timestamps;
    uint32_t capacity;
    uint32_t head;
    uint32_t count;
} reward_history_t;

/** Circular buffer for gradient statistics */
typedef struct {
    float* norms;
    uint32_t capacity;
    uint32_t head;
    uint32_t count;
    float running_mean;
    float running_var;
} gradient_history_t;

/** Loss history for spike detection */
typedef struct {
    float* values;
    uint32_t capacity;
    uint32_t head;
    uint32_t count;
    float running_mean;
} loss_history_t;

/** Checkpoint metadata */
typedef struct {
    int32_t id;
    uint64_t timestamp;
    size_t size;
    char metadata[NIMCP_ERROR_BUFFER_SIZE];
    bool valid;
} checkpoint_info_t;

/** Frozen parameter set */
typedef struct {
    uint32_t* indices;
    uint32_t count;
    uint32_t capacity;
} frozen_params_t;

/** Internal training guard structure */
struct training_guard_internal {
    uint32_t magic;                     /**< Magic number for validation */

    /* Configuration */
    training_guard_config_t config;

    /* Reward hacking detection */
    reward_history_t reward_history;    /**< Recent reward values */
    float reward_mean;                  /**< Running mean of rewards */
    float reward_variance;              /**< Running variance of rewards */

    /* Goal drift detection */
    float* reference_goal;              /**< Reference goal vector */
    uint32_t goal_dim;                  /**< Goal dimensionality */
    float cumulative_drift;             /**< Cumulative drift from reference */
    float drift_ewma;                   /**< EWMA of drift rate */

    /* Gradient anomaly detection */
    gradient_history_t gradient_history; /**< Gradient norm history */

    /* Loss monitoring */
    loss_history_t loss_history;        /**< Training loss history */

    /* Frozen parameters */
    frozen_params_t frozen_params;      /**< Set of frozen parameter indices */

    /* Checkpointing */
    checkpoint_info_t* checkpoints;     /**< Checkpoint metadata array */
    uint32_t checkpoint_capacity;       /**< Maximum checkpoints */
    uint32_t checkpoint_count;          /**< Current checkpoint count */
    int32_t next_checkpoint_id;         /**< Next checkpoint ID */
    uint64_t last_checkpoint_time;      /**< Last checkpoint timestamp */
    uint64_t updates_since_checkpoint;  /**< Updates since last checkpoint */

    /* Rollback tracking */
    uint32_t consecutive_violations;    /**< Consecutive violations count */

    /* Statistics */
    training_guard_stats_t stats;

    /* Security orchestrator */
    security_orchestrator_t orchestrator;
};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

static bool reward_history_init(reward_history_t* h, uint32_t capacity) {
    h->values = nimcp_calloc(capacity, sizeof(float));
    h->timestamps = nimcp_calloc(capacity, sizeof(uint64_t));
    if (!h->values || !h->timestamps) {
        nimcp_free(h->values);
        nimcp_free(h->timestamps);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "reward_history_init: required parameter is NULL (h->values, h->timestamps)");
        return false;
    }
    h->capacity = capacity;
    h->head = 0;
    h->count = 0;
    return true;
}

static void reward_history_destroy(reward_history_t* h) {
    nimcp_free(h->values);
    nimcp_free(h->timestamps);
    h->values = NULL;
    h->timestamps = NULL;
}

static void reward_history_add(reward_history_t* h, float value, uint64_t timestamp) {
    h->values[h->head] = value;
    h->timestamps[h->head] = timestamp;
    h->head = (h->head + 1) % h->capacity;
    if (h->count < h->capacity) h->count++;
}

static bool gradient_history_init(gradient_history_t* h, uint32_t capacity) {
    h->norms = nimcp_calloc(capacity, sizeof(float));
    if (!h->norms) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gradient_history_init: h->norms is NULL");
        return false;
    }
    h->capacity = capacity;
    h->head = 0;
    h->count = 0;
    h->running_mean = 0.0f;
    h->running_var = 0.0f;
    return true;
}

static void gradient_history_destroy(gradient_history_t* h) {
    nimcp_free(h->norms);
    h->norms = NULL;
}

static void gradient_history_add(gradient_history_t* h, float norm) {
    h->norms[h->head] = norm;
    h->head = (h->head + 1) % h->capacity;
    if (h->count < h->capacity) h->count++;

    /* Update running statistics */
    if (h->count > 1) {
        float delta = norm - h->running_mean;
        h->running_mean += delta / h->count;
        float delta2 = norm - h->running_mean;
        h->running_var += delta * delta2;
    } else {
        h->running_mean = norm;
        h->running_var = 0.0f;
    }
}

static float gradient_history_stddev(gradient_history_t* h) {
    if (h->count < 2) return 0.0f;
    return sqrtf(h->running_var / (h->count - 1));
}

static bool loss_history_init(loss_history_t* h, uint32_t capacity) {
    h->values = nimcp_calloc(capacity, sizeof(float));
    if (!h->values) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "loss_history_init: h->values is NULL");
        return false;
    }
    h->capacity = capacity;
    h->head = 0;
    h->count = 0;
    h->running_mean = 0.0f;
    return true;
}

static void loss_history_destroy(loss_history_t* h) {
    nimcp_free(h->values);
    h->values = NULL;
}

static void loss_history_add(loss_history_t* h, float loss) {
    h->values[h->head] = loss;
    h->head = (h->head + 1) % h->capacity;
    if (h->count < h->capacity) h->count++;

    /* Update running mean */
    float alpha = 0.1f;
    h->running_mean = alpha * loss + (1.0f - alpha) * h->running_mean;
}

static bool frozen_params_init(frozen_params_t* f, uint32_t capacity) {
    f->indices = nimcp_calloc(capacity, sizeof(uint32_t));
    if (!f->indices) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "frozen_params_init: f->indices is NULL");
        return false;
    }
    f->count = 0;
    f->capacity = capacity;
    return true;
}

static void frozen_params_destroy(frozen_params_t* f) {
    nimcp_free(f->indices);
    f->indices = NULL;
}

static bool frozen_params_contains(frozen_params_t* f, uint32_t index) {
    for (uint32_t i = 0; i < f->count; i++) {
        if (f->indices[i] == index) return true;
    }
    return false;
}

static bool frozen_params_add(frozen_params_t* f, uint32_t index) {
    if (f->count >= f->capacity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "frozen_params_add: capacity exceeded");
        return false;
    }
    if (frozen_params_contains(f, index)) {
        return false;
    }
    f->indices[f->count++] = index;
    return true;
}

static bool frozen_params_remove(frozen_params_t* f, uint32_t index) {
    for (uint32_t i = 0; i < f->count; i++) {
        if (f->indices[i] == index) {
            /* Shift remaining */
            for (uint32_t j = i; j < f->count - 1; j++) {
                f->indices[j] = f->indices[j + 1];
            }
            f->count--;
            return true;
        }
    }
    return false;
}

/**
 * Compute L2 norm of gradient vector
 */
static float compute_gradient_norm(const float* gradients, uint32_t size) {
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        sum_sq += gradients[i] * gradients[i];
    }
    return sqrtf(sum_sq);
}

/**
 * Check for NaN/Inf values in gradients
 */
static uint32_t count_nan_inf(const float* gradients, uint32_t size) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (isnan(gradients[i]) || isinf(gradients[i])) {
            count++;
        }
    }
    return count;
}

/**
 * Compute L2 distance between goal vectors
 */
static float compute_goal_distance(const float* a, const float* b, uint32_t dim) {
    float sum_sq = 0.0f;
    for (uint32_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum_sq += diff * diff;
    }
    return sqrtf(sum_sq);
}

/* ============================================================================
 * PUBLIC API IMPLEMENTATION
 * ============================================================================ */

training_guard_config_t training_guard_default_config(void) {
    training_guard_config_t config = {
        .gradient_clip_norm = TRAINING_DEFAULT_GRAD_CLIP_NORM,
        .gradient_clip_value = TRAINING_DEFAULT_GRAD_CLIP_VALUE,
        .enable_gradient_clipping = true,
        .gradient_anomaly_threshold = TRAINING_DEFAULT_ANOMALY_THRESHOLD,
        .enable_gradient_anomaly_detection = true,
        .anomaly_history_size = 100,
        .enable_reward_hacking_detection = true,
        .reward_hacking_sensitivity = 0.7f,
        .reward_history_size = TRAINING_REWARD_HISTORY_SIZE,
        .enable_goal_drift_detection = true,
        .goal_drift_threshold = TRAINING_DEFAULT_DRIFT_THRESHOLD,
        .goal_vector_dim = 0, /* Set when reference goal is provided */
        .drift_ewma_alpha = 0.1f,
        .enable_checkpointing = true,
        .checkpoint_interval = TRAINING_DEFAULT_CHECKPOINT_INTERVAL,
        .max_checkpoints = TRAINING_MAX_CHECKPOINTS,
        .checkpoint_dir = NULL,
        .enable_auto_rollback = false,
        .rollback_threshold_violations = 5,
        .frozen_param_indices = NULL,
        .num_frozen_params = 0,
        .enable_backdoor_detection = true,
        .backdoor_sensitivity = 0.5f,
        .enable_loss_monitoring = true,
        .loss_spike_threshold = 3.0f,
        .enable_violation_logging = true,
        .enable_statistics = true
    };
    return config;
}

training_guard_t training_guard_create(
    const training_guard_config_t* config,
    security_orchestrator_t orchestrator
) {
    struct training_guard_internal* guard = nimcp_calloc(1, sizeof(*guard));
    if (!guard) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_guard_create: allocation failed");

        return NULL;

    }

    guard->magic = LGSS_TRAINING_GUARD_MAGIC;

    if (config) {
        guard->config = *config;
    } else {
        guard->config = training_guard_default_config();
    }

    guard->orchestrator = orchestrator;

    /* Initialize reward history */
    if (!reward_history_init(&guard->reward_history, guard->config.reward_history_size)) {
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "training_guard_create: reward_history_init is NULL");
        return NULL;
    }

    /* Initialize gradient history */
    if (!gradient_history_init(&guard->gradient_history, guard->config.anomaly_history_size)) {
        reward_history_destroy(&guard->reward_history);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "training_guard_create: gradient_history_init is NULL");
        return NULL;
    }

    /* Initialize loss history */
    if (!loss_history_init(&guard->loss_history, 100)) {
        gradient_history_destroy(&guard->gradient_history);
        reward_history_destroy(&guard->reward_history);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "training_guard_create: loss_history_init is NULL");
        return NULL;
    }

    /* Initialize frozen parameters */
    if (!frozen_params_init(&guard->frozen_params, 256)) {
        loss_history_destroy(&guard->loss_history);
        gradient_history_destroy(&guard->gradient_history);
        reward_history_destroy(&guard->reward_history);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "training_guard_create: frozen_params_init is NULL");
        return NULL;
    }

    /* Add pre-configured frozen parameters */
    if (guard->config.frozen_param_indices && guard->config.num_frozen_params > 0) {
        for (uint32_t i = 0; i < guard->config.num_frozen_params; i++) {
            frozen_params_add(&guard->frozen_params, guard->config.frozen_param_indices[i]);
        }
    }

    /* Initialize checkpoint storage */
    guard->checkpoint_capacity = guard->config.max_checkpoints;
    guard->checkpoints = nimcp_calloc(guard->checkpoint_capacity, sizeof(checkpoint_info_t));
    if (!guard->checkpoints) {
        frozen_params_destroy(&guard->frozen_params);
        loss_history_destroy(&guard->loss_history);
        gradient_history_destroy(&guard->gradient_history);
        reward_history_destroy(&guard->reward_history);
        nimcp_free(guard);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "training_guard_create: checkpoints allocation failed");
        return NULL;
    }
    guard->next_checkpoint_id = 1;

    return guard;
}

void training_guard_destroy(training_guard_t guard) {
    if (!guard) return;

    struct training_guard_internal* g = guard;
    if (g->magic != LGSS_TRAINING_GUARD_MAGIC) return;

    nimcp_free(g->reference_goal);
    nimcp_free(g->checkpoints);
    frozen_params_destroy(&g->frozen_params);
    loss_history_destroy(&g->loss_history);
    gradient_history_destroy(&g->gradient_history);
    reward_history_destroy(&g->reward_history);

    g->magic = 0;
    nimcp_free(g);
}

int training_guard_reset(training_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    /* Reset histories */
    g->reward_history.head = 0;
    g->reward_history.count = 0;
    g->gradient_history.head = 0;
    g->gradient_history.count = 0;
    g->gradient_history.running_mean = 0.0f;
    g->gradient_history.running_var = 0.0f;
    g->loss_history.head = 0;
    g->loss_history.count = 0;
    g->loss_history.running_mean = 0.0f;

    /* Reset drift tracking */
    g->cumulative_drift = 0.0f;
    g->drift_ewma = 0.0f;

    /* Reset statistics */
    memset(&g->stats, 0, sizeof(g->stats));

    g->consecutive_violations = 0;
    g->updates_since_checkpoint = 0;

    return NIMCP_SUCCESS;
}

int training_guard_apply_gradients(
    training_guard_t guard,
    gradient_buffer_t* gradients,
    gradient_check_result_t* result
) {
    NIMCP_CHECK_THROW(guard && gradients && result && gradients->data, NIMCP_ERROR_NULL_POINTER, "guard, gradients, result, or gradients->data is NULL");

    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    uint64_t start_time = get_time_us();

    /* Initialize result */
    memset(result, 0, sizeof(*result));
    result->allowed = true;

    g->stats.total_updates_checked++;

    /* Check for NaN/Inf */
    uint32_t nan_count = count_nan_inf(gradients->data, gradients->size);
    if (nan_count > 0) {
        result->violations |= TRAINING_VIOLATION_NAN_DETECTED;
        result->nan_count = nan_count;
        result->allowed = false;
        snprintf(result->reason, sizeof(result->reason),
                "Found %u NaN/Inf values in gradients", nan_count);
        g->stats.nan_detections++;
        g->stats.updates_blocked++;
        return NIMCP_SUCCESS;
    }

    /* Compute gradient norm */
    float norm = compute_gradient_norm(gradients->data, gradients->size);
    result->original_norm = norm;
    result->clipped_norm = norm;

    /* Find max absolute value */
    float max_val = 0.0f;
    for (uint32_t i = 0; i < gradients->size; i++) {
        float abs_val = fabsf(gradients->data[i]);
        if (abs_val > max_val) max_val = abs_val;
    }
    result->max_value_seen = max_val;

    /* Check for frozen parameters */
    for (uint32_t i = 0; i < gradients->size && i < gradients->param_count; i++) {
        if (frozen_params_contains(&g->frozen_params, i)) {
            if (fabsf(gradients->data[i]) > 1e-8f) {
                result->violations |= TRAINING_VIOLATION_FROZEN_PARAM;
                g->stats.updates_blocked++; /* Count but don't block yet */
            }
        }
    }

    /* Gradient anomaly detection */
    if (g->config.enable_gradient_anomaly_detection && g->gradient_history.count > 10) {
        float stddev = gradient_history_stddev(&g->gradient_history);
        if (stddev > 0.0001f) {
            float z_score = (norm - g->gradient_history.running_mean) / stddev;
            result->anomaly_score = fabsf(z_score);

            if (result->anomaly_score > g->config.gradient_anomaly_threshold) {
                result->violations |= TRAINING_VIOLATION_GRAD_ANOMALY;
                g->stats.grad_anomaly_detections++;
                /* Flag but don't necessarily block */
            }
        }
    }

    /* Gradient clipping by norm */
    if (g->config.enable_gradient_clipping && norm > g->config.gradient_clip_norm) {
        result->violations |= TRAINING_VIOLATION_GRAD_NORM;
        g->stats.grad_norm_violations++;

        float scale = g->config.gradient_clip_norm / norm;
        for (uint32_t i = 0; i < gradients->size; i++) {
            gradients->data[i] *= scale;
        }
        result->clipped_norm = g->config.gradient_clip_norm;
        result->was_clipped = true;
    }

    /* Gradient clipping by value */
    uint32_t value_clips = 0;
    if (g->config.enable_gradient_clipping) {
        for (uint32_t i = 0; i < gradients->size; i++) {
            if (gradients->data[i] > g->config.gradient_clip_value) {
                gradients->data[i] = g->config.gradient_clip_value;
                value_clips++;
            } else if (gradients->data[i] < -g->config.gradient_clip_value) {
                gradients->data[i] = -g->config.gradient_clip_value;
                value_clips++;
            }
        }
        if (value_clips > 0) {
            result->violations |= TRAINING_VIOLATION_GRAD_VALUE;
            result->was_clipped = true;
            g->stats.grad_value_violations++;
        }
    }

    /* Zero out frozen parameters */
    for (uint32_t i = 0; i < gradients->size && i < gradients->param_count; i++) {
        if (frozen_params_contains(&g->frozen_params, i)) {
            gradients->data[i] = 0.0f;
        }
    }

    /* Store clipped gradients in result */
    result->clipped_gradients = gradients->data;

    /* Check for loss spike */
    if (g->config.enable_loss_monitoring && g->loss_history.count > 10) {
        if (gradients->current_loss > g->loss_history.running_mean * g->config.loss_spike_threshold) {
            result->violations |= TRAINING_VIOLATION_LOSS_SPIKE;
            /* Flag but don't block */
        }
    }

    /* Record loss */
    if (g->config.enable_loss_monitoring) {
        loss_history_add(&g->loss_history, gradients->current_loss);
    }

    /* Update gradient history */
    gradient_history_add(&g->gradient_history, result->clipped_norm);

    /* Update statistics */
    if (result->allowed) {
        g->stats.updates_allowed++;
        if (result->was_clipped) {
            g->stats.updates_clipped++;
        }
    }

    g->stats.avg_gradient_norm = (g->stats.avg_gradient_norm * (g->stats.total_updates_checked - 1) +
                                   result->clipped_norm) / g->stats.total_updates_checked;
    if (result->original_norm > g->stats.max_gradient_norm_seen) {
        g->stats.max_gradient_norm_seen = result->original_norm;
    }
    if (result->was_clipped) {
        g->stats.avg_clip_ratio = (g->stats.avg_clip_ratio * (g->stats.updates_clipped - 1) +
                                    result->clipped_norm / result->original_norm) /
                                   g->stats.updates_clipped;
    }

    /* Checkpointing */
    g->updates_since_checkpoint++;
    if (g->config.enable_checkpointing &&
        g->updates_since_checkpoint >= g->config.checkpoint_interval) {
        /* Would trigger checkpoint here - placeholder */
        g->updates_since_checkpoint = 0;
        g->stats.checkpoints_created++;
    }

    /* Track consecutive violations for auto-rollback */
    if (result->violations != TRAINING_VIOLATION_NONE) {
        g->consecutive_violations++;
    } else {
        g->consecutive_violations = 0;
    }

    /* Performance tracking */
    uint64_t elapsed = get_time_us() - start_time;
    g->stats.guard_overhead_us += elapsed;
    g->stats.avg_check_time_us = (float)g->stats.guard_overhead_us /
                                  (float)g->stats.total_updates_checked;

    return NIMCP_SUCCESS;
}

int training_guard_check_gradients(
    training_guard_t guard,
    const gradient_buffer_t* gradients,
    gradient_check_result_t* result
) {
    NIMCP_CHECK_THROW(guard && gradients && result, NIMCP_ERROR_NULL_POINTER, "guard, gradients, or result is NULL");

    /* Create a copy for non-destructive checking */
    gradient_buffer_t copy = *gradients;
    float* copy_data = nimcp_malloc(gradients->size * sizeof(float));
    NIMCP_CHECK_THROW(copy_data, NIMCP_ERROR_NO_MEMORY, "failed to allocate gradient buffer copy");

    memcpy(copy_data, gradients->data, gradients->size * sizeof(float));
    copy.data = copy_data;

    int err = training_guard_apply_gradients(guard, &copy, result);

    nimcp_free(copy_data);
    return err;
}

float training_guard_clip_by_norm(
    training_guard_t guard,
    float* gradients,
    uint32_t size,
    float max_norm
) {
    if (!guard || !gradients || size == 0) return 1.0f;

    float norm = compute_gradient_norm(gradients, size);
    if (norm <= max_norm) return 1.0f;

    float scale = max_norm / norm;
    for (uint32_t i = 0; i < size; i++) {
        gradients[i] *= scale;
    }
    return scale;
}

uint32_t training_guard_clip_by_value(
    training_guard_t guard,
    float* gradients,
    uint32_t size,
    float max_value
) {
    if (!guard || !gradients || size == 0) return 0;

    uint32_t clipped = 0;
    for (uint32_t i = 0; i < size; i++) {
        if (gradients[i] > max_value) {
            gradients[i] = max_value;
            clipped++;
        } else if (gradients[i] < -max_value) {
            gradients[i] = -max_value;
            clipped++;
        }
    }
    return clipped;
}

bool training_guard_detect_reward_hacking(
    training_guard_t guard,
    const reward_state_t* state,
    reward_hacking_result_t* result
) {
    if (!guard || !state || !result) {
        return false;
    }

    struct training_guard_internal* g = guard;
    if (g->magic != LGSS_TRAINING_GUARD_MAGIC) {
        return false;
    }

    memset(result, 0, sizeof(*result));

    if (!g->config.enable_reward_hacking_detection) {
        return false;
    }

    /* Need some history to detect anomalies */
    if (g->reward_history.count < 20) {
        return false;
    }

    float anomaly_score = 0.0f;

    /* Check 1: Reward variance collapse (possible self-reward) */
    if (state->reward_variance < 0.01f && g->reward_variance > 0.1f) {
        anomaly_score += 0.3f;
        result->type = REWARD_HACKING_SELF_REWARD;
    }

    /* Check 2: Sudden reward spike */
    if (state->current_reward > g->reward_mean + 3.0f * sqrtf(g->reward_variance)) {
        anomaly_score += 0.4f;
        if (result->type == REWARD_HACKING_NONE) {
            result->type = REWARD_HACKING_REWARD_TAMPERING;
        }
    }

    /* Check 3: Anomalous action-value correlation */
    /* Placeholder - would need more context */

    result->anomaly_score = anomaly_score;
    result->confidence = fminf(anomaly_score / 0.7f, 1.0f);
    result->detected = (anomaly_score > g->config.reward_hacking_sensitivity);

    if (result->detected) {
        snprintf(result->evidence, sizeof(result->evidence),
                "Anomaly score %.3f exceeds threshold %.3f. Type: %s",
                anomaly_score, g->config.reward_hacking_sensitivity,
                reward_hacking_type_name(result->type));
        g->stats.reward_hacking_detections++;
    }

    return result->detected;
}

int training_guard_record_reward(training_guard_t guard, float reward, uint64_t timestamp) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");

    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    if (timestamp == 0) {
        timestamp = get_time_us();
    }

    reward_history_add(&g->reward_history, reward, timestamp);

    /* Update running statistics */
    float alpha = 0.1f;
    float delta = reward - g->reward_mean;
    g->reward_mean += alpha * delta;
    g->reward_variance = (1.0f - alpha) * (g->reward_variance + alpha * delta * delta);

    g->stats.avg_reward = g->reward_mean;
    g->stats.reward_variance = g->reward_variance;

    return NIMCP_SUCCESS;
}

float training_guard_get_reward_anomaly_score(training_guard_t guard, float current_reward) {
    if (!guard) return 0.0f;

    struct training_guard_internal* g = guard;
    if (g->magic != LGSS_TRAINING_GUARD_MAGIC) return 0.0f;

    if (g->reward_variance < 0.0001f) return 0.0f;

    float z_score = (current_reward - g->reward_mean) / sqrtf(g->reward_variance);
    return fabsf(z_score);
}

bool training_guard_detect_goal_drift(
    training_guard_t guard,
    const float* current_goal,
    uint32_t goal_dim,
    goal_drift_result_t* result
) {
    if (!guard || !current_goal || !result) {
        return false;
    }

    struct training_guard_internal* g = guard;
    if (g->magic != LGSS_TRAINING_GUARD_MAGIC) {
        return false;
    }

    memset(result, 0, sizeof(*result));

    if (!g->config.enable_goal_drift_detection || !g->reference_goal) {
        return false;  /* Detection disabled or no reference - normal behavior */
    }

    if (goal_dim != g->goal_dim) {
        return false;  /* Dimension mismatch - cannot compare */
    }

    /* Compute distance from reference */
    float drift = compute_goal_distance(current_goal, g->reference_goal, goal_dim);
    result->drift_magnitude = drift;

    /* Update drift EWMA */
    float drift_delta = drift - g->cumulative_drift;
    g->drift_ewma = g->config.drift_ewma_alpha * drift_delta +
                    (1.0f - g->config.drift_ewma_alpha) * g->drift_ewma;
    result->drift_rate = g->drift_ewma;

    g->cumulative_drift = drift;

    /* Detect drift type */
    if (drift > g->config.goal_drift_threshold) {
        result->detected = true;

        if (fabsf(drift_delta) > 0.1f) {
            result->type = GOAL_DRIFT_SUDDEN;
        } else if (g->drift_ewma > 0.01f) {
            result->type = GOAL_DRIFT_DIVERGENT;
        } else {
            result->type = GOAL_DRIFT_GRADUAL;
        }

        snprintf(result->description, sizeof(result->description),
                "Goal drift detected: magnitude %.4f, rate %.4f",
                drift, g->drift_ewma);
        g->stats.goal_drift_detections++;
        g->stats.cumulative_drift = drift;
        if (drift > g->stats.max_drift_seen) {
            g->stats.max_drift_seen = drift;
        }
    }

    return result->detected;
}

int training_guard_set_reference_goal(
    training_guard_t guard,
    const float* goal,
    uint32_t goal_dim
) {
    NIMCP_CHECK_THROW(guard && goal && goal_dim > 0, NIMCP_ERROR_NULL_POINTER, "guard or goal is NULL, or goal_dim is zero");

    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    NIMCP_CHECK_THROW(goal_dim <= TRAINING_MAX_GOAL_DIM, NIMCP_ERROR_OUT_OF_RANGE, "goal_dim exceeds maximum");

    /* Allocate or reallocate */
    float* new_goal = nimcp_realloc(g->reference_goal, goal_dim * sizeof(float));
    NIMCP_CHECK_THROW(new_goal, NIMCP_ERROR_NO_MEMORY, "failed to allocate reference goal");

    memcpy(new_goal, goal, goal_dim * sizeof(float));
    g->reference_goal = new_goal;
    g->goal_dim = goal_dim;
    g->cumulative_drift = 0.0f;
    g->drift_ewma = 0.0f;

    return NIMCP_SUCCESS;
}

int training_guard_get_goal_drift(
    training_guard_t guard,
    const float* current_goal,
    uint32_t goal_dim,
    float* drift_out
) {
    NIMCP_CHECK_THROW(guard && current_goal && drift_out, NIMCP_ERROR_NULL_POINTER, "guard, current_goal, or drift_out is NULL");

    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    if (!g->reference_goal || goal_dim != g->goal_dim) {
        *drift_out = 0.0f;
        NIMCP_CHECK_THROW(false, NIMCP_ERROR_INVALID_STATE, "no reference goal or dimension mismatch");
    }

    *drift_out = compute_goal_distance(current_goal, g->reference_goal, goal_dim);
    return NIMCP_SUCCESS;
}

int32_t training_guard_create_checkpoint(
    training_guard_t guard,
    const void* model_data,
    size_t model_size,
    const char* metadata
) {
    if (!guard) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "training_guard_create_checkpoint: guard is NULL");
        return -1;
    }

    struct training_guard_internal* g = guard;
    if (g->magic != LGSS_TRAINING_GUARD_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "training_guard_create_checkpoint: validation failed");
        return -1;
    }

    /* Placeholder - would save to disk */
    (void)model_data;
    (void)model_size;

    /* Find slot (circular buffer) */
    uint32_t slot = (g->checkpoint_count < g->checkpoint_capacity) ?
                    g->checkpoint_count : (g->next_checkpoint_id - 1) % g->checkpoint_capacity;

    checkpoint_info_t* cp = &g->checkpoints[slot];
    cp->id = g->next_checkpoint_id++;
    cp->timestamp = get_time_us();
    cp->size = model_size;
    cp->valid = true;

    if (metadata) {
        strncpy(cp->metadata, metadata, sizeof(cp->metadata) - 1);
        cp->metadata[sizeof(cp->metadata) - 1] = '\0';
    }

    if (g->checkpoint_count < g->checkpoint_capacity) {
        g->checkpoint_count++;
    }

    g->last_checkpoint_time = cp->timestamp;
    g->stats.checkpoints_created++;

    return cp->id;
}

int training_guard_rollback(
    training_guard_t guard,
    int32_t checkpoint_id,
    void* model_data,
    size_t model_size,
    size_t* actual_size
) {
    NIMCP_CHECK_THROW(guard && model_data, NIMCP_ERROR_NULL_POINTER, "guard or model_data is NULL");

    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    /* Placeholder - would load from disk */
    (void)checkpoint_id;
    (void)model_size;
    if (actual_size) *actual_size = 0;

    g->stats.rollbacks_performed++;
    return NIMCP_SUCCESS;
}

uint32_t training_guard_get_checkpoint_count(training_guard_t guard) {
    if (!guard) return 0;
    struct training_guard_internal* g = guard;
    if (g->magic != LGSS_TRAINING_GUARD_MAGIC) return 0;
    return g->checkpoint_count;
}

uint32_t training_guard_prune_checkpoints(training_guard_t guard, uint32_t keep_count) {
    if (!guard) return 0;
    struct training_guard_internal* g = guard;
    if (g->magic != LGSS_TRAINING_GUARD_MAGIC) return 0;

    if (keep_count >= g->checkpoint_count) return 0;

    uint32_t pruned = g->checkpoint_count - keep_count;
    g->checkpoint_count = keep_count;
    return pruned;
}

int training_guard_freeze_parameter(training_guard_t guard, uint32_t param_index) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    NIMCP_CHECK_THROW(frozen_params_add(&g->frozen_params, param_index), NIMCP_ERROR_OUT_OF_RANGE, "failed to freeze parameter");
    return NIMCP_SUCCESS;
}

int training_guard_unfreeze_parameter(training_guard_t guard, uint32_t param_index) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    NIMCP_CHECK_THROW(frozen_params_remove(&g->frozen_params, param_index), NIMCP_ERROR_NOT_FOUND, "parameter not found for unfreezing");
    return NIMCP_SUCCESS;
}

bool training_guard_is_parameter_frozen(training_guard_t guard, uint32_t param_index) {
    if (!guard) return false;
    struct training_guard_internal* g = guard;
    if (g->magic != LGSS_TRAINING_GUARD_MAGIC) {
        return false;
    }
    return frozen_params_contains(&g->frozen_params, param_index);
}

int training_guard_get_stats(training_guard_t guard, training_guard_stats_t* stats) {
    NIMCP_CHECK_THROW(guard && stats, NIMCP_ERROR_NULL_POINTER, "guard or stats is NULL");
    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    *stats = g->stats;
    return NIMCP_SUCCESS;
}

int training_guard_reset_stats(training_guard_t guard) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    memset(&g->stats, 0, sizeof(g->stats));
    return NIMCP_SUCCESS;
}

int training_guard_record_loss(training_guard_t guard, float loss) {
    NIMCP_CHECK_THROW(guard, NIMCP_ERROR_NULL_POINTER, "guard is NULL");
    struct training_guard_internal* g = guard;
    NIMCP_CHECK_THROW(g->magic == LGSS_TRAINING_GUARD_MAGIC, NIMCP_ERROR_INVALID_STATE, "invalid training guard magic");

    loss_history_add(&g->loss_history, loss);
    return NIMCP_SUCCESS;
}

const char* training_violation_name(training_violation_t violation) {
    switch (violation) {
        case TRAINING_VIOLATION_NONE: return "NONE";
        case TRAINING_VIOLATION_GRAD_NORM: return "GRAD_NORM";
        case TRAINING_VIOLATION_GRAD_VALUE: return "GRAD_VALUE";
        case TRAINING_VIOLATION_GRAD_ANOMALY: return "GRAD_ANOMALY";
        case TRAINING_VIOLATION_REWARD_HACKING: return "REWARD_HACKING";
        case TRAINING_VIOLATION_GOAL_DRIFT: return "GOAL_DRIFT";
        case TRAINING_VIOLATION_CATASTROPHIC: return "CATASTROPHIC";
        case TRAINING_VIOLATION_BACKDOOR: return "BACKDOOR";
        case TRAINING_VIOLATION_FROZEN_PARAM: return "FROZEN_PARAM";
        case TRAINING_VIOLATION_LOSS_SPIKE: return "LOSS_SPIKE";
        case TRAINING_VIOLATION_NAN_DETECTED: return "NAN_DETECTED";
        default: return "UNKNOWN";
    }
}

const char* reward_hacking_type_name(reward_hacking_type_t type) {
    switch (type) {
        case REWARD_HACKING_NONE: return "NONE";
        case REWARD_HACKING_REWARD_TAMPERING: return "REWARD_TAMPERING";
        case REWARD_HACKING_SHORTCUT_BEHAVIOR: return "SHORTCUT_BEHAVIOR";
        case REWARD_HACKING_SENSOR_MANIPULATION: return "SENSOR_MANIPULATION";
        case REWARD_HACKING_SELF_REWARD: return "SELF_REWARD";
        case REWARD_HACKING_REWARD_CORRELATION: return "REWARD_CORRELATION";
        case REWARD_HACKING_GOAL_SUBSTITUTION: return "GOAL_SUBSTITUTION";
        default: return "UNKNOWN";
    }
}

const char* goal_drift_type_name(goal_drift_type_t type) {
    switch (type) {
        case GOAL_DRIFT_NONE: return "NONE";
        case GOAL_DRIFT_GRADUAL: return "GRADUAL";
        case GOAL_DRIFT_SUDDEN: return "SUDDEN";
        case GOAL_DRIFT_OSCILLATING: return "OSCILLATING";
        case GOAL_DRIFT_DIVERGENT: return "DIVERGENT";
        default: return "UNKNOWN";
    }
}

void training_guard_print_summary(training_guard_t guard) {
    if (!guard) {
        printf("Training Guard: NULL\n");
        return;
    }

    struct training_guard_internal* g = guard;
    if (g->magic != LGSS_TRAINING_GUARD_MAGIC) {
        printf("Training Guard: INVALID (bad magic)\n");
        return;
    }

    printf("=== Training Guard Summary ===\n");
    printf("Updates checked: %lu\n", (unsigned long)g->stats.total_updates_checked);
    printf("Allowed: %lu, Blocked: %lu, Clipped: %lu\n",
           (unsigned long)g->stats.updates_allowed,
           (unsigned long)g->stats.updates_blocked,
           (unsigned long)g->stats.updates_clipped);
    printf("Avg gradient norm: %.4f (max: %.4f)\n",
           g->stats.avg_gradient_norm, g->stats.max_gradient_norm_seen);
    printf("Frozen parameters: %u\n", g->frozen_params.count);
    printf("Checkpoints: %u created, %u rollbacks\n",
           g->stats.checkpoints_created, g->stats.rollbacks_performed);
    printf("Reward hacking detections: %lu\n",
           (unsigned long)g->stats.reward_hacking_detections);
    printf("Goal drift detections: %lu (max drift: %.4f)\n",
           (unsigned long)g->stats.goal_drift_detections, g->stats.max_drift_seen);
    printf("==============================\n");
}
