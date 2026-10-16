/**
 * @file nimcp_pr_training_plasticity.c
 * @brief Training-Plasticity Extension Implementation
 * @version 1.0.0
 * @date 2026-01-09
 *
 * WHAT: Implementation of integration layer between PR memory plasticity
 *       and gradient-based training
 * WHY:  Enable unified learning combining biological plasticity with
 *       modern ML optimization techniques
 * HOW:  Implements gradient<->STDP conversion, hybrid loss computation,
 *       and epoch consolidation mechanisms
 */

#include "cognitive/memory/core/nimcp_pr_training_plasticity.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stdio.h>
#include <float.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(pr_training_plasticity, MESH_ADAPTER_CATEGORY_MEMORY)


//=============================================================================
// Internal Structure Definition
//=============================================================================

/**
 * @brief Internal Training-Plasticity structure
 *
 * WHAT: Complete state for training-plasticity integration
 * WHY:  Encapsulate all bridge data for thread safety
 */
struct pr_training_plasticity_struct {
    /* Configuration */
    pr_training_plasticity_config_t config;

    /* Connected bridges */
    pr_plasticity_bridge_t plasticity_bridge;
    z_ladder_t z_ladder;
    entangle_graph_t graph;
    bool plasticity_connected;
    bool z_ladder_connected;
    bool graph_connected;

    /* Current state */
    pr_training_phase_t current_phase;
    uint32_t steps_in_phase;
    uint32_t current_epoch;
    uint64_t total_steps;

    /* Conversion buffers */
    pr_stdp_timing_t* stdp_buffer;
    size_t stdp_buffer_capacity;
    pr_pseudo_gradient_t* pseudo_grad_buffer;
    size_t pseudo_grad_buffer_capacity;

    /* Loss tracking */
    float last_supervised_loss;
    float last_unsupervised_loss;
    float loss_ema;  /* Exponential moving average */
    float loss_variance_ema;

    /* Per-epoch tracking */
    uint64_t epoch_access_count;
    double epoch_gradient_sum;
    double epoch_plasticity_sum;
    uint32_t epoch_step_count;

    /* Adaptive weight state */
    float current_supervised_weight;
    float current_unsupervised_weight;

    /* Statistics */
    pr_training_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;
};

//=============================================================================
// Helper Functions
//=============================================================================

/**
 * @brief Sign function
 */
static inline float sign_f(float x) {
    return (x > 0.0f) ? 1.0f : (x < 0.0f) ? -1.0f : 0.0f;
}

/**
 * @brief Check if float is NaN
 */
static inline bool is_nan_f(float x) {
    return x != x;  /* NaN != NaN is true */
}

/**
 * @brief Compute exponential moving average
 */
static inline float ema_update(float current_ema, float new_value, float alpha) {
    return alpha * new_value + (1.0f - alpha) * current_ema;
}

/**
 * @brief Convert gradient to STDP timing (linear method)
 */
static float gradient_to_timing_linear(
    float gradient,
    float timing_scale,
    float max_delta,
    float min_delta)
{
    float delta = gradient * timing_scale;
    float abs_delta = fabsf(delta);

    if (abs_delta < min_delta) {
        return 0.0f;  /* Below threshold, no STDP */
    }

    return nimcp_clampf(delta, -max_delta, max_delta);
}

/**
 * @brief Convert gradient to STDP timing (tanh method)
 */
static float gradient_to_timing_tanh(
    float gradient,
    float max_delta)
{
    return tanhf(gradient) * max_delta;
}

/**
 * @brief Convert gradient to STDP timing (sign method)
 */
static float gradient_to_timing_sign(
    float gradient,
    float fixed_delta,
    float min_magnitude)
{
    if (fabsf(gradient) < min_magnitude) {
        return 0.0f;
    }
    return sign_f(gradient) * fixed_delta;
}

/**
 * @brief Convert plasticity event to pseudo-gradient (direct method)
 */
static float event_to_gradient_direct(float delta_weight) {
    /* LTP (positive delta) -> negative gradient (gradient descent wants to increase) */
    /* Actually, gradient descent wants to MINIMIZE loss, so:
     * - If plasticity increased weight (good), gradient should be negative
     * - If plasticity decreased weight, gradient should be positive
     * This is because optimizer will subtract gradient from weights
     */
    return -delta_weight;
}

/**
 * @brief Convert plasticity event to pseudo-gradient (scaled method)
 */
static float event_to_gradient_scaled(float delta_weight, float learning_rate) {
    return -delta_weight * learning_rate;
}

/**
 * @brief Convert plasticity event to pseudo-gradient (resonance method)
 */
static float event_to_gradient_resonance(
    float delta_weight,
    float resonance,
    float resonance_weight)
{
    float modulation = 1.0f + resonance_weight * resonance;
    return -delta_weight * modulation;
}

/**
 * @brief Allocate conversion buffers
 */
static int allocate_buffers(pr_training_plasticity_t tp) {
    /* STDP buffer */
    tp->stdp_buffer_capacity = tp->config.max_gradients_per_step;
    tp->stdp_buffer = nimcp_calloc(tp->stdp_buffer_capacity, sizeof(pr_stdp_timing_t));
    if (!tp->stdp_buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_buffers: tp->stdp_buffer is NULL");
        return -1;
    }

    /* Pseudo-gradient buffer */
    tp->pseudo_grad_buffer_capacity = tp->config.max_events_per_step;
    tp->pseudo_grad_buffer = nimcp_calloc(tp->pseudo_grad_buffer_capacity,
                                          sizeof(pr_pseudo_gradient_t));
    if (!tp->pseudo_grad_buffer) {
        nimcp_free(tp->stdp_buffer);
        tp->stdp_buffer = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "allocate_buffers: tp->pseudo_grad_buffer is NULL");
        return -1;
    }

    return 0;
}

/**
 * @brief Free conversion buffers
 */
static void free_buffers(pr_training_plasticity_t tp) {
    if (tp->stdp_buffer) {
        nimcp_free(tp->stdp_buffer);
        tp->stdp_buffer = NULL;
    }
    if (tp->pseudo_grad_buffer) {
        nimcp_free(tp->pseudo_grad_buffer);
        tp->pseudo_grad_buffer = NULL;
    }
}

/**
 * @brief Initialize statistics
 */
static void init_stats(pr_training_stats_t* stats) {
    memset(stats, 0, sizeof(pr_training_stats_t));
    stats->min_loss = FLT_MAX;
    stats->max_loss = -FLT_MAX;
}

/**
 * @brief Update step statistics
 */
static void update_step_stats(
    pr_training_plasticity_t tp,
    const pr_training_step_result_t* result)
{
    tp->stats.total_steps++;
    tp->stats.total_weights_updated += result->weights_updated;
    tp->stats.total_gradient_contribution += result->gradient_contribution;
    tp->stats.total_plasticity_contribution += result->plasticity_contribution;

    if (result->phase == PR_PHASE_GRADIENT) {
        tp->stats.gradient_steps++;
    } else if (result->phase == PR_PHASE_PLASTICITY) {
        tp->stats.plasticity_steps++;
    } else if (result->phase == PR_PHASE_CONSOLIDATION) {
        tp->stats.consolidation_steps++;
    }

    /* Update loss statistics */
    if (!is_nan_f(result->step_loss)) {
        if (result->step_loss < tp->stats.min_loss) {
            tp->stats.min_loss = result->step_loss;
        }
        if (result->step_loss > tp->stats.max_loss) {
            tp->stats.max_loss = result->step_loss;
        }
        tp->stats.loss_sum += result->step_loss;

        /* Update loss history ring buffer */
        tp->stats.loss_history[tp->stats.loss_history_idx] = result->step_loss;
        tp->stats.loss_history_idx = (tp->stats.loss_history_idx + 1) % PR_TRAIN_STATS_HISTORY_LEN;
    }

    /* Update timing */
    tp->stats.total_training_time_ms += result->step_time_ms;
    tp->stats.avg_step_time_ms = tp->stats.total_training_time_ms / tp->stats.total_steps;

    /* Update mode counts */
    if (tp->config.mode < PR_TRAINING_MODE_COUNT) {
        tp->stats.mode_step_counts[tp->config.mode]++;
    }
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_training_plasticity_config_t pr_training_plasticity_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__config_default", 0.0f);


    pr_training_plasticity_config_t config;
    memset(&config, 0, sizeof(config));

    /* Mode configuration */
    config.mode = PR_TRAINING_UNIFIED;
    config.gradient_weight = PR_TRAIN_DEFAULT_GRADIENT_WEIGHT;
    config.plasticity_weight = PR_TRAIN_DEFAULT_PLASTICITY_WEIGHT;
    config.alternation_period = PR_TRAIN_DEFAULT_ALTERNATION_PERIOD;
    config.consolidate_after_epoch = true;

    /* Gradient -> STDP conversion */
    config.grad_to_stdp_method = PR_GRAD_TO_STDP_LINEAR;
    config.timing_scale = PR_TRAIN_DEFAULT_TIMING_SCALE;
    config.max_stdp_delta = PR_TRAIN_MAX_STDP_DELTA;
    config.min_stdp_delta = PR_TRAIN_MIN_STDP_DELTA;

    /* STDP -> Gradient conversion */
    config.stdp_to_grad_method = PR_STDP_TO_GRAD_SCALED;
    config.pseudo_learning_rate = PR_TRAIN_DEFAULT_PSEUDO_LR;
    config.resonance_weight = PR_TRAIN_DEFAULT_RESONANCE_WEIGHT;

    /* Hybrid loss configuration */
    config.supervised_weight = PR_TRAIN_DEFAULT_SUPERVISED_WEIGHT;
    config.unsupervised_weight = PR_TRAIN_DEFAULT_UNSUPERVISED_WEIGHT;
    config.adaptive_loss_weights = false;
    config.bcm_loss_weight = 0.25f;
    config.homeostatic_loss_weight = 0.25f;
    config.resonance_loss_weight = 0.3f;
    config.sparsity_loss_weight = 0.1f;
    config.consolidation_loss_weight = 0.1f;

    /* Epoch consolidation */
    config.consolidation_strength_threshold = 0.5f;
    config.consolidation_batch_size = PR_TRAIN_CONSOLIDATION_BATCH_SIZE;
    config.sleep_consolidation = false;

    /* Performance tuning */
    config.max_gradients_per_step = PR_TRAIN_MAX_GRADIENT_BATCH;
    config.max_events_per_step = PR_TRAIN_MAX_EVENTS_PER_STEP;
    config.enable_statistics = true;
    config.verbose_logging = false;

    return config;
}

bool pr_training_plasticity_config_validate(
    const pr_training_plasticity_config_t* config)
{
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_plasticity_config_validate: config is NULL");
        return false;
    }

    /* Mode must be valid */
    if (config->mode >= PR_TRAINING_MODE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_training_plasticity_config_validate: capacity exceeded");
        return false;
    }

    /* Weights must be valid */
    if (config->gradient_weight < 0.0f || config->gradient_weight > 1.0f) {
        return false;
    }
    if (config->plasticity_weight < 0.0f || config->plasticity_weight > 1.0f) {
        return false;
    }
    if (config->gradient_weight + config->plasticity_weight < PR_TRAIN_EPSILON) {
        return false;
    }

    /* Timing scale must be positive */
    if (config->timing_scale <= 0.0f) {
        return false;
    }

    /* STDP delta range must be valid */
    if (config->min_stdp_delta < 0.0f) {
        return false;
    }
    if (config->max_stdp_delta <= config->min_stdp_delta) {
        return false;
    }

    /* Pseudo learning rate must be positive */
    if (config->pseudo_learning_rate <= 0.0f) {
        return false;
    }

    /* Loss weights must be valid */
    if (config->supervised_weight < 0.0f) {
        return false;
    }
    if (config->unsupervised_weight < 0.0f) {
        return false;
    }
    if (config->supervised_weight + config->unsupervised_weight < PR_TRAIN_EPSILON) {
        return false;
    }

    /* Alternation period must be positive if alternating */
    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__config_validate", 0.0f);


    if (config->mode == PR_TRAINING_ALTERNATING && config->alternation_period == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_training_plasticity_config_validate: config->alternation_period is zero");
        return false;
    }

    /* Buffer sizes must be reasonable */
    if (config->max_gradients_per_step == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_training_plasticity_config_validate: config->max_gradients_per_step is zero");
        return false;
    }
    if (config->max_events_per_step == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_training_plasticity_config_validate: config->max_events_per_step is zero");
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pr_training_plasticity_t pr_training_plasticity_create(
    const pr_training_plasticity_config_t* config)
{
    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__create", 0.0f);


    pr_training_plasticity_t tp = nimcp_calloc(1, sizeof(struct pr_training_plasticity_struct));
    if (!tp) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate tp");

        return NULL;

    }

    /* Apply configuration */
    if (config) {
        if (!pr_training_plasticity_config_validate(config)) {
            nimcp_free(tp);
            tp = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_plasticity_create: pr_training_plasticity_config_validate is NULL");
            return NULL;
        }
        tp->config = *config;
    } else {
        tp->config = pr_training_plasticity_config_default();
    }

    /* Create mutex */
    mutex_attr_t mutex_attr = {.type = MUTEX_TYPE_NORMAL};
    tp->mutex = nimcp_mutex_create(&mutex_attr);
    if (!tp->mutex) {
        nimcp_free(tp);
        tp = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_training_plasticity_create: tp->mutex is NULL");
        return NULL;
    }

    /* Allocate conversion buffers */
    if (allocate_buffers(tp) != 0) {
        nimcp_mutex_destroy(tp->mutex);
        nimcp_free(tp);
        tp = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_training_plasticity_create: validation failed");
        return NULL;
    }

    /* Initialize state */
    tp->current_phase = PR_PHASE_GRADIENT;
    tp->steps_in_phase = 0;
    tp->current_epoch = 0;
    tp->total_steps = 0;

    tp->plasticity_connected = false;
    tp->z_ladder_connected = false;
    tp->graph_connected = false;

    tp->last_supervised_loss = 0.0f;
    tp->last_unsupervised_loss = 0.0f;
    tp->loss_ema = 0.0f;
    tp->loss_variance_ema = 0.0f;

    tp->current_supervised_weight = tp->config.supervised_weight;
    tp->current_unsupervised_weight = tp->config.unsupervised_weight;

    /* Initialize statistics */
    init_stats(&tp->stats);

    return tp;
}

void pr_training_plasticity_destroy(pr_training_plasticity_t tp) {
    if (!tp) return;

    /* Free buffers */
    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__destroy", 0.0f);


    free_buffers(tp);

    /* Destroy mutex */
    if (tp->mutex) {
        nimcp_mutex_destroy(tp->mutex);
    }

    nimcp_free(tp);
    tp = NULL;
}

int pr_training_plasticity_reset(pr_training_plasticity_t tp) {
    if (!tp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_plasticity_reset: tp is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__reset", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    /* Reset state */
    tp->current_phase = PR_PHASE_GRADIENT;
    tp->steps_in_phase = 0;
    tp->total_steps = 0;

    tp->last_supervised_loss = 0.0f;
    tp->last_unsupervised_loss = 0.0f;
    tp->loss_ema = 0.0f;
    tp->loss_variance_ema = 0.0f;

    tp->epoch_access_count = 0;
    tp->epoch_gradient_sum = 0.0;
    tp->epoch_plasticity_sum = 0.0;
    tp->epoch_step_count = 0;

    tp->current_supervised_weight = tp->config.supervised_weight;
    tp->current_unsupervised_weight = tp->config.unsupervised_weight;

    /* Reset statistics */
    init_stats(&tp->stats);

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_plasticity_connect(
    pr_training_plasticity_t tp,
    pr_plasticity_bridge_t plasticity_bridge)
{
    if (!tp || !plasticity_bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_plasticity_connect: required parameter is NULL (tp, plasticity_bridge)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__connect", 0.0f);


    nimcp_mutex_lock(tp->mutex);
    tp->plasticity_bridge = plasticity_bridge;
    tp->plasticity_connected = true;
    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_plasticity_connect_z_ladder(
    pr_training_plasticity_t tp,
    z_ladder_t z_ladder)
{
    if (!tp || !z_ladder) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_plasticity_connect_z_ladder: required parameter is NULL (tp, z_ladder)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__connect_z_ladder", 0.0f);


    nimcp_mutex_lock(tp->mutex);
    tp->z_ladder = z_ladder;
    tp->z_ladder_connected = true;
    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_plasticity_connect_graph(
    pr_training_plasticity_t tp,
    entangle_graph_t graph)
{
    if (!tp || !graph) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_plasticity_connect_graph: required parameter is NULL (tp, graph)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__connect_graph", 0.0f);


    nimcp_mutex_lock(tp->mutex);
    tp->graph = graph;
    tp->graph_connected = true;
    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

//=============================================================================
// Gradient <-> STDP Conversion Functions
//=============================================================================

int pr_training_gradient_to_stdp(
    pr_training_plasticity_t tp,
    const pr_gradient_element_t* gradients,
    size_t grad_count,
    pr_stdp_timing_t* timing,
    size_t max_timing,
    size_t* timing_count)
{
    if (!tp || !gradients || !timing || !timing_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_gradient_to_stdp: required parameter is NULL (tp, gradients, timing, timing_count)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_gradient", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    size_t count = 0;
    size_t limit = (grad_count < max_timing) ? grad_count : max_timing;

    for (size_t i = 0; i < limit; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && limit > 256) {
            pr_training_plasticity_heartbeat("pr_training__loop",
                             (float)(i + 1) / (float)limit);
        }

        float delta_t = 0.0f;

        switch (tp->config.grad_to_stdp_method) {
            case PR_GRAD_TO_STDP_LINEAR:
                delta_t = gradient_to_timing_linear(
                    gradients[i].gradient,
                    tp->config.timing_scale,
                    tp->config.max_stdp_delta,
                    tp->config.min_stdp_delta);
                break;

            case PR_GRAD_TO_STDP_TANH:
                delta_t = gradient_to_timing_tanh(
                    gradients[i].gradient,
                    tp->config.max_stdp_delta);
                break;

            case PR_GRAD_TO_STDP_SIGN:
                delta_t = gradient_to_timing_sign(
                    gradients[i].gradient,
                    tp->config.timing_scale,
                    0.01f);  /* Minimum magnitude threshold */
                break;

            case PR_GRAD_TO_STDP_ADAPTIVE:
                /* Adaptive: scale by gradient statistics */
                {
                    float scale = tp->config.timing_scale;
                    /* Increase scale for small gradients, decrease for large */
                    float abs_grad = fabsf(gradients[i].gradient);
                    if (abs_grad > 1.0f) {
                        scale /= sqrtf(abs_grad);
                    } else if (abs_grad < 0.1f && abs_grad > 0.0f) {
                        scale *= sqrtf(0.1f / abs_grad);
                    }
                    delta_t = gradient_to_timing_linear(
                        gradients[i].gradient, scale,
                        tp->config.max_stdp_delta,
                        tp->config.min_stdp_delta);
                }
                break;

            default:
                delta_t = gradient_to_timing_linear(
                    gradients[i].gradient,
                    tp->config.timing_scale,
                    tp->config.max_stdp_delta,
                    tp->config.min_stdp_delta);
                break;
        }

        /* Skip zero timing (below threshold) */
        if (fabsf(delta_t) < PR_TRAIN_EPSILON) {
            continue;
        }

        /* Create timing event */
        timing[count].from_node = gradients[i].param_id;
        timing[count].to_node = gradients[i].param_id;  /* Self-connection for now */
        timing[count].delta_t_ms = delta_t;

        /* Compute expected weight change from STDP rule */
        /* Simplified: positive delta_t -> LTP -> positive weight change */
        if (delta_t > 0) {
            /* Pre before post -> LTP */
            timing[count].target_delta_weight = 0.1f * expf(-delta_t / 20.0f);
        } else {
            /* Post before pre -> LTD */
            timing[count].target_delta_weight = -0.12f * expf(delta_t / 20.0f);
        }

        timing[count].resonance = 0.5f;  /* Default resonance */

        count++;
    }

    *timing_count = count;

    /* Update statistics */
    tp->stats.gradients_converted += count;

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_plasticity_to_grad(
    pr_training_plasticity_t tp,
    const pr_plasticity_event_t* events,
    size_t event_count,
    pr_pseudo_gradient_t* pseudo_grads,
    size_t max_grads,
    size_t* grad_count)
{
    if (!tp || !events || !pseudo_grads || !grad_count) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_plasticity_to_grad: required parameter is NULL (tp, events, pseudo_grads, grad_count)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__to_grad", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    size_t count = 0;
    size_t limit = (event_count < max_grads) ? event_count : max_grads;

    for (size_t i = 0; i < limit; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && limit > 256) {
            pr_training_plasticity_heartbeat("pr_training__loop",
                             (float)(i + 1) / (float)limit);
        }

        float pseudo_grad = 0.0f;
        float confidence = 1.0f;

        switch (tp->config.stdp_to_grad_method) {
            case PR_STDP_TO_GRAD_DIRECT:
                pseudo_grad = event_to_gradient_direct(events[i].delta_weight);
                break;

            case PR_STDP_TO_GRAD_SCALED:
                pseudo_grad = event_to_gradient_scaled(
                    events[i].delta_weight,
                    tp->config.pseudo_learning_rate);
                break;

            case PR_STDP_TO_GRAD_RESONANCE:
                pseudo_grad = event_to_gradient_resonance(
                    events[i].delta_weight,
                    events[i].resonance_at_event,
                    tp->config.resonance_weight);
                /* Higher resonance -> higher confidence */
                confidence = 0.5f + 0.5f * events[i].resonance_at_event;
                break;

            case PR_STDP_TO_GRAD_WEIGHTED:
                {
                    /* Weight by event type and tier (if available) */
                    float type_weight = 1.0f;
                    switch (events[i].type) {
                        case PR_PLASTICITY_STDP:
                            type_weight = 1.0f;
                            break;
                        case PR_PLASTICITY_BCM:
                            type_weight = 0.8f;
                            break;
                        case PR_PLASTICITY_HOMEOSTATIC:
                            type_weight = 0.5f;
                            break;
                        case PR_PLASTICITY_STRUCTURAL:
                            type_weight = 0.3f;
                            break;
                        default:
                            type_weight = 0.5f;
                            break;
                    }
                    pseudo_grad = event_to_gradient_scaled(
                        events[i].delta_weight * type_weight,
                        tp->config.pseudo_learning_rate);
                    confidence = type_weight;
                }
                break;

            default:
                pseudo_grad = event_to_gradient_direct(events[i].delta_weight);
                break;
        }

        /* Skip very small pseudo-gradients */
        if (fabsf(pseudo_grad) < PR_TRAIN_EPSILON) {
            continue;
        }

        pseudo_grads[count].param_id = events[i].from_node;
        pseudo_grads[count].pseudo_gradient = pseudo_grad;
        pseudo_grads[count].confidence = confidence;
        pseudo_grads[count].source = events[i].type;

        count++;
    }

    *grad_count = count;

    /* Update statistics */
    tp->stats.events_converted += count;
    if (count > 0) {
        float total_confidence = 0.0f;
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)count);
            }

            total_confidence += pseudo_grads[i].confidence;
        }
        /* Update running average */
        float avg_conf = total_confidence / (float)count;
        tp->stats.avg_conversion_confidence =
            (tp->stats.avg_conversion_confidence * 0.99) + (avg_conf * 0.01);
    }

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

float pr_training_apply_stdp_timing(
    pr_training_plasticity_t tp,
    entangle_graph_t graph,
    const pr_stdp_timing_t* timing)
{
    if (!tp || !graph || !timing) return -1.0f;

    /* Use plasticity bridge if connected */
    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_apply_st", 0.0f);


    if (tp->plasticity_connected && tp->plasticity_bridge) {
        /* Convert timing to pre/post spike times */
        float pre_time = 0.0f;
        float post_time = timing->delta_t_ms;

        return pr_stdp_apply_to_entanglement(
            tp->plasticity_bridge,
            graph,
            timing->from_node,
            timing->to_node,
            pre_time,
            post_time,
            timing->resonance);
    }

    /* Manual STDP application if no bridge */
    entangle_edge_t edge;
    if (!entangle_get_edge(graph, timing->from_node, timing->to_node, &edge)) {
        return -1.0f;
    }

    float old_weight = edge.weight;
    edge.weight = nimcp_clampf(edge.weight + timing->target_delta_weight, 0.0f, 1.0f);

    if (fabsf(edge.weight - old_weight) > PR_TRAIN_EPSILON) {
        entangle_update_edge(graph, &edge);
    }

    return edge.weight;
}

uint32_t pr_training_batch_apply_stdp(
    pr_training_plasticity_t tp,
    entangle_graph_t graph,
    const pr_stdp_timing_t* timing,
    size_t count)
{
    if (!tp || !graph || !timing) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_batch_ap", 0.0f);


    uint32_t updated = 0;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_training_plasticity_heartbeat("pr_training__loop",
                             (float)(i + 1) / (float)count);
        }

        float new_weight = pr_training_apply_stdp_timing(tp, graph, &timing[i]);
        if (new_weight >= 0.0f) {
            updated++;
        }
    }

    return updated;
}

//=============================================================================
// Core Training Functions
//=============================================================================

int pr_training_unified_step(
    pr_training_plasticity_t tp,
    pr_model_interface_t* model,
    const pr_training_batch_t* batch,
    pr_memory_node_t** memory,
    size_t memory_count,
    pr_training_step_result_t* result)
{
    if (!tp || !model || !batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_unified_step: required parameter is NULL (tp, model, batch)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_unified_", 0.0f);


    uint64_t start_time_us = nimcp_time_get_us();

    nimcp_mutex_lock(tp->mutex);

    pr_training_step_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));
    local_result.phase = tp->current_phase;

    /* Step 1: Compute loss */
    float loss = 0.0f;
    if (model->compute_loss) {
        loss = model->compute_loss(model->model_handle, batch->data);
        local_result.step_loss = loss;
    }

    /* Step 2: Get gradients from model */
    pr_gradient_element_t* gradients = nimcp_calloc(
        tp->config.max_gradients_per_step,
        sizeof(pr_gradient_element_t));
    if (!gradients) {
        nimcp_mutex_unlock(tp->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_training_unified_step: gradients is NULL");
        return -1;
    }

    size_t grad_count = 0;
    if (model->get_gradients) {
        model->get_gradients(model->model_handle, gradients,
                            tp->config.max_gradients_per_step, &grad_count);
    }

    /* Step 3: Convert gradients to STDP timing */
    size_t stdp_count = 0;
    if (grad_count > 0 && tp->config.plasticity_weight > 0.0f) {
        pr_training_gradient_to_stdp(tp, gradients, grad_count,
                                     tp->stdp_buffer, tp->stdp_buffer_capacity,
                                     &stdp_count);
    }

    /* Step 4: Apply STDP updates if graph connected */
    uint32_t stdp_updated = 0;
    if (stdp_count > 0 && tp->graph_connected && tp->graph) {
        stdp_updated = pr_training_batch_apply_stdp(tp, tp->graph,
                                                    tp->stdp_buffer, stdp_count);
    }

    /* Step 5: Get plasticity events and convert to pseudo-gradients */
    size_t pseudo_grad_count = 0;
    if (tp->plasticity_connected && tp->plasticity_bridge &&
        tp->config.plasticity_weight > 0.0f) {
        pr_plasticity_event_t events[1024];
        uint32_t event_count = 0;
        pr_plasticity_get_events(tp->plasticity_bridge, events, 1024, &event_count);

        if (event_count > 0) {
            pr_training_plasticity_to_grad(tp, events, event_count,
                                           tp->pseudo_grad_buffer,
                                           tp->pseudo_grad_buffer_capacity,
                                           &pseudo_grad_count);
        }
    }

    /* Step 6: Blend gradient and plasticity updates */
    float total_grad_contribution = 0.0f;
    float total_plast_contribution = 0.0f;

    if (tp->config.mode == PR_TRAINING_UNIFIED) {
        /* Unified: apply both weighted */
        /* Scale gradients by gradient_weight */
        for (size_t i = 0; i < grad_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && grad_count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)grad_count);
            }

            gradients[i].gradient *= tp->config.gradient_weight;
            total_grad_contribution += fabsf(gradients[i].gradient);
        }

        /* Add pseudo-gradients scaled by plasticity_weight */
        for (size_t i = 0; i < pseudo_grad_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pseudo_grad_count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)pseudo_grad_count);
            }

            /* Find matching gradient and add */
            for (size_t j = 0; j < grad_count; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && grad_count > 256) {
                    pr_training_plasticity_heartbeat("pr_training__loop",
                                     (float)(j + 1) / (float)grad_count);
                }

                if (gradients[j].param_id == tp->pseudo_grad_buffer[i].param_id) {
                    float plast_contrib = tp->pseudo_grad_buffer[i].pseudo_gradient *
                                         tp->config.plasticity_weight;
                    gradients[j].gradient += plast_contrib;
                    total_plast_contribution += fabsf(plast_contrib);
                    break;
                }
            }
        }
    } else if (tp->config.mode == PR_TRAINING_GRADIENT_PRIMARY) {
        /* Gradient primary: plasticity as regularizer */
        for (size_t i = 0; i < grad_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && grad_count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)grad_count);
            }

            total_grad_contribution += fabsf(gradients[i].gradient);
        }
        /* Plasticity contributes to regularization, not gradients */
        total_plast_contribution = (float)stdp_updated * 0.01f;
    } else if (tp->config.mode == PR_TRAINING_PLASTICITY_PRIMARY) {
        /* Plasticity primary: use pseudo-gradients primarily */
        for (size_t i = 0; i < grad_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && grad_count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)grad_count);
            }

            gradients[i].gradient *= 0.1f;  /* Small gradient contribution */
            total_grad_contribution += fabsf(gradients[i].gradient);
        }
        for (size_t i = 0; i < pseudo_grad_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pseudo_grad_count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)pseudo_grad_count);
            }

            /* Find matching gradient and replace */
            for (size_t j = 0; j < grad_count; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && grad_count > 256) {
                    pr_training_plasticity_heartbeat("pr_training__loop",
                                     (float)(j + 1) / (float)grad_count);
                }

                if (gradients[j].param_id == tp->pseudo_grad_buffer[i].param_id) {
                    gradients[j].gradient = tp->pseudo_grad_buffer[i].pseudo_gradient;
                    total_plast_contribution += fabsf(gradients[j].gradient);
                    break;
                }
            }
        }
    } else if (tp->config.mode == PR_TRAINING_HYBRID) {
        /* Hybrid: adaptive blend */
        /* Use loss EMA to adjust weights */
        float loss_ratio = 1.0f;
        if (tp->loss_ema > PR_TRAIN_EPSILON) {
            loss_ratio = loss / tp->loss_ema;
        }

        /* High loss ratio (getting worse) -> increase supervised weight */
        /* Low loss ratio (improving) -> allow more plasticity */
        float adaptive_grad_weight = tp->config.gradient_weight;
        float adaptive_plast_weight = tp->config.plasticity_weight;

        if (loss_ratio > 1.1f) {
            adaptive_grad_weight *= 1.2f;
            adaptive_plast_weight *= 0.8f;
        } else if (loss_ratio < 0.9f) {
            adaptive_grad_weight *= 0.9f;
            adaptive_plast_weight *= 1.1f;
        }

        /* Normalize */
        float total = adaptive_grad_weight + adaptive_plast_weight;
        adaptive_grad_weight /= total;
        adaptive_plast_weight /= total;

        for (size_t i = 0; i < grad_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && grad_count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)grad_count);
            }

            gradients[i].gradient *= adaptive_grad_weight;
            total_grad_contribution += fabsf(gradients[i].gradient);
        }

        for (size_t i = 0; i < pseudo_grad_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && pseudo_grad_count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)pseudo_grad_count);
            }

            for (size_t j = 0; j < grad_count; j++) {
                /* Phase 8: Loop progress heartbeat */
                if ((j & 0xFF) == 0 && grad_count > 256) {
                    pr_training_plasticity_heartbeat("pr_training__loop",
                                     (float)(j + 1) / (float)grad_count);
                }

                if (gradients[j].param_id == tp->pseudo_grad_buffer[i].param_id) {
                    float plast_contrib = tp->pseudo_grad_buffer[i].pseudo_gradient *
                                         adaptive_plast_weight;
                    gradients[j].gradient += plast_contrib;
                    total_plast_contribution += fabsf(plast_contrib);
                    break;
                }
            }
        }
    }

    /* Step 7: Apply updates to model */
    uint32_t weights_updated = 0;
    if (model->apply_updates && grad_count > 0) {
        model->apply_updates(model->model_handle, gradients, grad_count);
        weights_updated = (uint32_t)grad_count;
    }

    /* Step 8: Update memory nodes if provided */
    if (memory && memory_count > 0) {
        for (size_t i = 0; i < memory_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && memory_count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)memory_count);
            }

            if (memory[i]) {
                /* Record access */
                tp->epoch_access_count++;
            }
        }
    }

    /* Step 9: Update loss EMA */
    if (!is_nan_f(loss)) {
        float alpha = 0.1f;
        if (tp->loss_ema < PR_TRAIN_EPSILON) {
            tp->loss_ema = loss;
        } else {
            tp->loss_ema = ema_update(tp->loss_ema, loss, alpha);
        }

        float variance = (loss - tp->loss_ema) * (loss - tp->loss_ema);
        tp->loss_variance_ema = ema_update(tp->loss_variance_ema, variance, alpha);
    }

    /* Fill result */
    local_result.weights_updated = weights_updated + stdp_updated;
    local_result.total_weight_change = total_grad_contribution + total_plast_contribution;

    float total_contrib = total_grad_contribution + total_plast_contribution;
    if (total_contrib > PR_TRAIN_EPSILON) {
        local_result.gradient_contribution = total_grad_contribution / total_contrib;
        local_result.plasticity_contribution = total_plast_contribution / total_contrib;
    } else {
        local_result.gradient_contribution = 0.5f;
        local_result.plasticity_contribution = 0.5f;
    }

    /* Update epoch tracking */
    tp->epoch_gradient_sum += total_grad_contribution;
    tp->epoch_plasticity_sum += total_plast_contribution;
    tp->epoch_step_count++;

    /* Timing */
    uint64_t end_time_us = nimcp_time_get_us();
    local_result.step_time_ms = (float)(end_time_us - start_time_us) / 1000.0f;

    /* Update total steps */
    tp->total_steps++;
    tp->steps_in_phase++;

    /* Update statistics */
    if (tp->config.enable_statistics) {
        update_step_stats(tp, &local_result);
    }

    /* Free gradient buffer */
    nimcp_free(gradients);
    gradients = NULL;

    if (result) {
        *result = local_result;
    }

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_alternating_step(
    pr_training_plasticity_t tp,
    pr_model_interface_t* model,
    const pr_training_batch_t* batch,
    bool is_gradient_phase,
    pr_training_step_result_t* result)
{
    if (!tp || !model || !batch) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_alternating_step: required parameter is NULL (tp, model, batch)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_alternat", 0.0f);


    uint64_t start_time_us = nimcp_time_get_us();

    nimcp_mutex_lock(tp->mutex);

    pr_training_step_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));
    local_result.phase = is_gradient_phase ? PR_PHASE_GRADIENT : PR_PHASE_PLASTICITY;

    if (is_gradient_phase) {
        /* Gradient phase: standard backprop */
        float loss = 0.0f;
        if (model->compute_loss) {
            loss = model->compute_loss(model->model_handle, batch->data);
            local_result.step_loss = loss;
        }

        pr_gradient_element_t* gradients = nimcp_calloc(
            tp->config.max_gradients_per_step,
            sizeof(pr_gradient_element_t));
        if (!gradients) {
            nimcp_mutex_unlock(tp->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_training_alternating_step: gradients is NULL");
            return -1;
        }

        size_t grad_count = 0;
        if (model->get_gradients) {
            model->get_gradients(model->model_handle, gradients,
                                tp->config.max_gradients_per_step, &grad_count);
        }

        if (model->apply_updates && grad_count > 0) {
            model->apply_updates(model->model_handle, gradients, grad_count);
            local_result.weights_updated = (uint32_t)grad_count;
        }

        float total_grad = 0.0f;
        for (size_t i = 0; i < grad_count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && grad_count > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)grad_count);
            }

            total_grad += fabsf(gradients[i].gradient);
        }
        local_result.total_weight_change = total_grad;
        local_result.gradient_contribution = 1.0f;
        local_result.plasticity_contribution = 0.0f;

        nimcp_free(gradients);
        gradients = NULL;

    } else {
        /* Plasticity phase: STDP/BCM only */
        if (tp->plasticity_connected && tp->plasticity_bridge && tp->graph_connected) {
            /* Update plasticity bridge */
            float dt_ms = 1.0f;  /* Assume 1ms timestep */
            pr_plasticity_bridge_update(tp->plasticity_bridge, tp->graph, dt_ms);

            /* Get events that occurred */
            pr_plasticity_event_t events[1024];
            uint32_t event_count = 0;
            pr_plasticity_get_events(tp->plasticity_bridge, events, 1024, &event_count);

            local_result.weights_updated = event_count;
            local_result.gradient_contribution = 0.0f;
            local_result.plasticity_contribution = 1.0f;

            float total_change = 0.0f;
            for (uint32_t i = 0; i < event_count; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && event_count > 256) {
                    pr_training_plasticity_heartbeat("pr_training__loop",
                                     (float)(i + 1) / (float)event_count);
                }

                total_change += fabsf(events[i].delta_weight);
            }
            local_result.total_weight_change = total_change;
        }
    }

    /* Timing */
    uint64_t end_time_us = nimcp_time_get_us();
    local_result.step_time_ms = (float)(end_time_us - start_time_us) / 1000.0f;

    /* Update step counters */
    tp->total_steps++;
    tp->steps_in_phase++;

    /* Update statistics */
    if (tp->config.enable_statistics) {
        update_step_stats(tp, &local_result);
    }

    if (result) {
        *result = local_result;
    }

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

pr_training_phase_t pr_training_advance_phase(pr_training_plasticity_t tp) {
    if (!tp) return PR_PHASE_GRADIENT;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_advance_", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    if (tp->config.mode == PR_TRAINING_ALTERNATING) {
        if (tp->steps_in_phase >= tp->config.alternation_period) {
            /* Switch phase */
            if (tp->current_phase == PR_PHASE_GRADIENT) {
                tp->current_phase = PR_PHASE_PLASTICITY;
            } else {
                tp->current_phase = PR_PHASE_GRADIENT;
            }
            tp->steps_in_phase = 0;
        }
    }

    pr_training_phase_t phase = tp->current_phase;

    nimcp_mutex_unlock(tp->mutex);

    return phase;
}

//=============================================================================
// Hybrid Loss Functions
//=============================================================================

int pr_training_hybrid_loss(
    pr_training_plasticity_t tp,
    float supervised_loss,
    float unsupervised_loss,
    pr_hybrid_loss_t* loss)
{
    if (!tp || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_hybrid_loss: required parameter is NULL (tp, loss)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_hybrid_l", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    memset(loss, 0, sizeof(pr_hybrid_loss_t));

    /* Supervised loss */
    loss->supervised_loss = supervised_loss;
    loss->component_losses[PR_LOSS_SUPERVISED] = supervised_loss;

    /* Unsupervised loss */
    if (is_nan_f(unsupervised_loss)) {
        /* Compute from plasticity state */
        float computed_unsup = 0.0f;
        pr_training_compute_unsupervised_loss(tp, &computed_unsup);
        loss->unsupervised_loss = computed_unsup;
    } else {
        loss->unsupervised_loss = unsupervised_loss;
    }

    /* Use current weights (may be adaptive) */
    loss->supervised_weight = tp->current_supervised_weight;
    loss->unsupervised_weight = tp->current_unsupervised_weight;

    /* Compute total */
    loss->total_loss = loss->supervised_weight * loss->supervised_loss +
                       loss->unsupervised_weight * loss->unsupervised_loss;

    /* Store for tracking */
    tp->last_supervised_loss = supervised_loss;
    tp->last_unsupervised_loss = loss->unsupervised_loss;

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_compute_unsupervised_loss(
    pr_training_plasticity_t tp,
    float* loss)
{
    if (!tp || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_compute_unsupervised_loss: required parameter is NULL (tp, loss)");
        return -1;
    }

    /* Note: In full implementation, would query actual plasticity state */
    /* For now, compute placeholder values */

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_compute_", 0.0f);


    float bcm_loss = 0.0f;
    float homeostatic_loss = 0.0f;
    float resonance_loss = 0.0f;
    float sparsity_loss = 0.0f;
    float consolidation_loss = 0.0f;

    if (tp->plasticity_connected && tp->plasticity_bridge) {
        pr_plasticity_bridge_stats_t stats;
        if (pr_plasticity_get_stats(tp->plasticity_bridge, &stats) == 0) {
            /* BCM loss: based on avg activity deviation (placeholder) */
            bcm_loss = 0.1f;  /* Would compute from BCM state */

            /* Homeostatic loss: based on tier activity deviations */
            homeostatic_loss = 0.05f;  /* Would compute from tier activities */

            /* Resonance loss: 1 - average resonance (want high resonance) */
            resonance_loss = 1.0f - stats.avg_resonance_modulation;
        }
    }

    /* Sparsity loss: based on activation statistics */
    sparsity_loss = 0.05f;  /* Placeholder */

    /* Consolidation loss: stability of consolidated memories */
    if (tp->z_ladder_connected && tp->z_ladder) {
        z_ladder_stats_t z_stats;
        if (z_ladder_get_stats(tp->z_ladder, &z_stats) == Z_LADDER_SUCCESS) {
            /* More demotions than promotions -> higher loss */
            float demotion_rate = 0.0f;
            float promotion_rate = 0.0f;
            for (int i = 0; i < 3; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && 3 > 256) {
                    pr_training_plasticity_heartbeat("pr_training__loop",
                                     (float)(i + 1) / (float)3);
                }

                demotion_rate += z_stats.demotions[i];
                promotion_rate += z_stats.promotions[i];
            }
            if (promotion_rate + demotion_rate > 0) {
                consolidation_loss = demotion_rate / (promotion_rate + demotion_rate);
            }
        }
    }

    /* Weighted combination */
    *loss = tp->config.bcm_loss_weight * bcm_loss +
            tp->config.homeostatic_loss_weight * homeostatic_loss +
            tp->config.resonance_loss_weight * resonance_loss +
            tp->config.sparsity_loss_weight * sparsity_loss +
            tp->config.consolidation_loss_weight * consolidation_loss;

    return 0;
}

float pr_training_compute_loss_component(
    pr_training_plasticity_t tp,
    pr_loss_component_t component)
{
    if (!tp) return NAN;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_compute_", 0.0f);


    float value = 0.0f;

    nimcp_mutex_lock(tp->mutex);

    switch (component) {
        case PR_LOSS_SUPERVISED:
            value = tp->last_supervised_loss;
            break;

        case PR_LOSS_BCM_ENERGY:
            /* Would compute from BCM state */
            value = 0.1f;
            break;

        case PR_LOSS_HOMEOSTATIC:
            /* Would compute from activity state */
            value = 0.05f;
            break;

        case PR_LOSS_RESONANCE:
            if (tp->plasticity_connected && tp->plasticity_bridge) {
                pr_plasticity_bridge_stats_t stats;
                if (pr_plasticity_get_stats(tp->plasticity_bridge, &stats) == 0) {
                    value = 1.0f - stats.avg_resonance_modulation;
                }
            }
            break;

        case PR_LOSS_SPARSITY:
            value = 0.05f;
            break;

        case PR_LOSS_CONSOLIDATION:
            value = 0.1f;
            break;

        default:
            value = NAN;
            break;
    }

    nimcp_mutex_unlock(tp->mutex);

    return value;
}

int pr_training_update_loss_weights(
    pr_training_plasticity_t tp,
    const float* recent_loss,
    size_t loss_count)
{
    if (!tp || !recent_loss || loss_count == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_update_loss_weights: required parameter is NULL (tp, recent_loss)");
        return -1;
    }
    if (!tp->config.adaptive_loss_weights) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_update_l", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    /* Compute loss statistics */
    float mean = 0.0f;
    for (size_t i = 0; i < loss_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && loss_count > 256) {
            pr_training_plasticity_heartbeat("pr_training__loop",
                             (float)(i + 1) / (float)loss_count);
        }

        mean += recent_loss[i];
    }
    mean /= (float)loss_count;

    float variance = 0.0f;
    for (size_t i = 0; i < loss_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && loss_count > 256) {
            pr_training_plasticity_heartbeat("pr_training__loop",
                             (float)(i + 1) / (float)loss_count);
        }

        float diff = recent_loss[i] - mean;
        variance += diff * diff;
    }
    variance /= (float)loss_count;

    /* Compute coefficient of variation */
    float cv = (mean > PR_TRAIN_EPSILON) ? sqrtf(variance) / (fabsf(mean) > 1e-7f ? mean : 1e-7f) : 0.0f;

    /* Adaptive logic:
     * - High CV (unstable): increase supervised weight
     * - Low CV (stable): can increase unsupervised weight
     */
    float alpha = 0.1f;  /* Adaptation rate */

    if (cv > 0.3f) {
        /* Unstable, favor supervised */
        tp->current_supervised_weight += alpha * (1.0f - tp->current_supervised_weight);
        tp->current_unsupervised_weight -= alpha * tp->current_unsupervised_weight;
    } else if (cv < 0.1f) {
        /* Stable, allow more unsupervised */
        tp->current_supervised_weight -= alpha * tp->current_supervised_weight * 0.5f;
        tp->current_unsupervised_weight += alpha * (1.0f - tp->current_unsupervised_weight) * 0.5f;
    }

    /* Ensure minimum weights */
    tp->current_supervised_weight = nimcp_clampf(tp->current_supervised_weight, 0.1f, 0.95f);
    tp->current_unsupervised_weight = nimcp_clampf(tp->current_unsupervised_weight, 0.05f, 0.9f);

    /* Normalize */
    float total = tp->current_supervised_weight + tp->current_unsupervised_weight;
    tp->current_supervised_weight /= total;
    tp->current_unsupervised_weight /= total;

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

//=============================================================================
// Epoch and Consolidation Functions
//=============================================================================

int pr_training_epoch_consolidate(
    pr_training_plasticity_t tp,
    pr_epoch_consolidation_result_t* result)
{
    if (!tp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_epoch_consolidate: tp is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_epoch_co", 0.0f);


    uint64_t start_time_ms = nimcp_time_get_ms();

    nimcp_mutex_lock(tp->mutex);

    pr_epoch_consolidation_result_t local_result;
    memset(&local_result, 0, sizeof(local_result));

    /* Record pre-consolidation state */
    local_result.pre_consolidation_loss = tp->loss_ema;

    /* Consolidate using Z-Ladder if connected */
    if (tp->z_ladder_connected && tp->z_ladder) {
        z_ladder_stats_t pre_stats;
        z_ladder_get_stats(tp->z_ladder, &pre_stats);

        /* Run consolidation */
        if (tp->config.sleep_consolidation) {
            z_ladder_sleep_consolidate(tp->z_ladder);
        } else {
            z_ladder_consolidate(tp->z_ladder);
        }

        z_ladder_stats_t post_stats;
        z_ladder_get_stats(tp->z_ladder, &post_stats);

        /* Compute changes */
        for (int i = 0; i < 3; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && 3 > 256) {
                pr_training_plasticity_heartbeat("pr_training__loop",
                                 (float)(i + 1) / (float)3);
            }

            local_result.memories_promoted += (uint32_t)(post_stats.promotions[i] - pre_stats.promotions[i]);
            local_result.memories_demoted += (uint32_t)(post_stats.demotions[i] - pre_stats.demotions[i]);
        }
    }

    /* Structural plasticity if connected */
    if (tp->plasticity_connected && tp->plasticity_bridge && tp->graph_connected) {
        pr_plasticity_bridge_stats_t pre_stats;
        pr_plasticity_get_stats(tp->plasticity_bridge, &pre_stats);

        /* Could trigger structural remodeling here */
        /* For now, just update the bridge */
        pr_plasticity_bridge_update(tp->plasticity_bridge, tp->graph, 1000.0f);

        pr_plasticity_bridge_stats_t post_stats;
        pr_plasticity_get_stats(tp->plasticity_bridge, &post_stats);

        local_result.edges_strengthened = (uint32_t)(post_stats.stdp_ltp_events - pre_stats.stdp_ltp_events);
        local_result.edges_pruned = (uint32_t)(post_stats.edges_pruned - pre_stats.edges_pruned);
    }

    /* Timing */
    uint64_t end_time_ms = nimcp_time_get_ms();
    local_result.consolidation_time_ms = (float)(end_time_ms - start_time_ms);

    /* Post-consolidation loss (would need to recompute) */
    local_result.post_consolidation_loss = tp->loss_ema;  /* Placeholder */

    /* Update epoch statistics */
    tp->stats.epochs_completed++;
    tp->stats.total_promotions += local_result.memories_promoted;
    tp->stats.total_demotions += local_result.memories_demoted;

    /* Reset per-epoch tracking */
    tp->epoch_access_count = 0;
    tp->epoch_gradient_sum = 0.0;
    tp->epoch_plasticity_sum = 0.0;
    tp->epoch_step_count = 0;

    if (result) {
        *result = local_result;
    }

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_epoch_start(
    pr_training_plasticity_t tp,
    uint32_t epoch_number)
{
    if (!tp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_epoch_start: tp is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_epoch_st", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    tp->current_epoch = epoch_number;
    tp->epoch_access_count = 0;
    tp->epoch_gradient_sum = 0.0;
    tp->epoch_plasticity_sum = 0.0;
    tp->epoch_step_count = 0;

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_epoch_end(
    pr_training_plasticity_t tp,
    bool trigger_consolidation)
{
    if (!tp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_epoch_end: tp is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_epoch_en", 0.0f);


    if (trigger_consolidation || tp->config.consolidate_after_epoch) {
        return pr_training_epoch_consolidate(tp, NULL);
    }

    return 0;
}

//=============================================================================
// State Query Functions
//=============================================================================

pr_training_phase_t pr_training_get_phase(pr_training_plasticity_t tp) {
    if (!tp) return PR_PHASE_GRADIENT;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_get_phas", 0.0f);


    nimcp_mutex_lock(tp->mutex);
    pr_training_phase_t phase = tp->current_phase;
    nimcp_mutex_unlock(tp->mutex);

    return phase;
}

int pr_training_set_mode(
    pr_training_plasticity_t tp,
    pr_training_mode_t mode)
{
    if (!tp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_set_mode: tp is NULL");
        return -1;
    }
    if (mode >= PR_TRAINING_MODE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "pr_training_set_mode: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_set_mode", 0.0f);


    nimcp_mutex_lock(tp->mutex);
    tp->config.mode = mode;

    /* Reset phase state when changing mode */
    tp->current_phase = PR_PHASE_GRADIENT;
    tp->steps_in_phase = 0;

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

pr_training_mode_t pr_training_get_mode(pr_training_plasticity_t tp) {
    if (!tp) return PR_TRAINING_UNIFIED;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_get_mode", 0.0f);


    nimcp_mutex_lock(tp->mutex);
    pr_training_mode_t mode = tp->config.mode;
    nimcp_mutex_unlock(tp->mutex);

    return mode;
}

int pr_training_get_stats(
    pr_training_plasticity_t tp,
    pr_training_stats_t* stats)
{
    if (!tp || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_get_stats: required parameter is NULL (tp, stats)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_get_stat", 0.0f);


    nimcp_mutex_lock(tp->mutex);
    *stats = tp->stats;
    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_reset_stats(pr_training_plasticity_t tp) {
    if (!tp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_reset_stats: tp is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_reset_st", 0.0f);


    nimcp_mutex_lock(tp->mutex);
    init_stats(&tp->stats);
    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_get_config(
    pr_training_plasticity_t tp,
    pr_training_plasticity_config_t* config)
{
    if (!tp || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_get_config: required parameter is NULL (tp, config)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_get_conf", 0.0f);


    nimcp_mutex_lock(tp->mutex);
    *config = tp->config;
    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_set_config(
    pr_training_plasticity_t tp,
    const pr_training_plasticity_config_t* config)
{
    if (!tp || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_set_config: required parameter is NULL (tp, config)");
        return -1;
    }
    if (!pr_training_plasticity_config_validate(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "pr_training_set_config: pr_training_plasticity_config_validate is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_set_conf", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    /* Save old buffer sizes */
    size_t old_grad_cap = tp->config.max_gradients_per_step;
    size_t old_event_cap = tp->config.max_events_per_step;

    tp->config = *config;

    /* Reallocate buffers if sizes changed */
    if (config->max_gradients_per_step != old_grad_cap ||
        config->max_events_per_step != old_event_cap) {
        free_buffers(tp);
        if (allocate_buffers(tp) != 0) {
            /* Restore old sizes and try again */
            tp->config.max_gradients_per_step = (uint32_t)old_grad_cap;
            tp->config.max_events_per_step = (uint32_t)old_event_cap;
            allocate_buffers(tp);
            nimcp_mutex_unlock(tp->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_training_set_config: validation failed");
            return -1;
        }
    }

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

//=============================================================================
// Synchronization Functions
//=============================================================================

int pr_training_sync_all(pr_training_plasticity_t tp) {
    if (!tp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_sync_all: tp is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_sync_all", 0.0f);


    int result = 0;

    result |= pr_training_sync_plasticity(tp);
    result |= pr_training_sync_z_ladder(tp);

    return result;
}

int pr_training_sync_plasticity(pr_training_plasticity_t tp) {
    if (!tp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_sync_plasticity: tp is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_sync_pla", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    if (tp->plasticity_connected && tp->plasticity_bridge) {
        pr_plasticity_sync_with_coordinator(tp->plasticity_bridge);
    }

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

int pr_training_sync_z_ladder(pr_training_plasticity_t tp) {
    if (!tp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_sync_z_ladder: tp is NULL");
        return -1;
    }

    /* Z-Ladder doesn't have explicit sync, just validate */
    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_sync_z_l", 0.0f);


    nimcp_mutex_lock(tp->mutex);

    if (tp->z_ladder_connected && tp->z_ladder) {
        z_ladder_validate(tp->z_ladder);
    }

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_training_mode_name(pr_training_mode_t mode) {
    switch (mode) {
        case PR_TRAINING_UNIFIED:           return "UNIFIED";
        case PR_TRAINING_ALTERNATING:       return "ALTERNATING";
        case PR_TRAINING_GRADIENT_PRIMARY:  return "GRADIENT_PRIMARY";
        case PR_TRAINING_PLASTICITY_PRIMARY: return "PLASTICITY_PRIMARY";
        case PR_TRAINING_HYBRID:            return "HYBRID";
        default:                            return "UNKNOWN";
    }
}

const char* pr_training_phase_name(pr_training_phase_t phase) {
    switch (phase) {
        case PR_PHASE_GRADIENT:      return "GRADIENT";
        case PR_PHASE_PLASTICITY:    return "PLASTICITY";
        case PR_PHASE_CONSOLIDATION: return "CONSOLIDATION";
        case PR_PHASE_EVALUATION:    return "EVALUATION";
        default:                     return "UNKNOWN";
    }
}

const char* pr_loss_component_name(pr_loss_component_t component) {
    switch (component) {
        case PR_LOSS_SUPERVISED:     return "SUPERVISED";
        case PR_LOSS_BCM_ENERGY:     return "BCM_ENERGY";
        case PR_LOSS_HOMEOSTATIC:    return "HOMEOSTATIC";
        case PR_LOSS_RESONANCE:      return "RESONANCE";
        case PR_LOSS_SPARSITY:       return "SPARSITY";
        case PR_LOSS_CONSOLIDATION:  return "CONSOLIDATION";
        default:                     return "UNKNOWN";
    }
}

void pr_training_print_stats(pr_training_plasticity_t tp) {
    if (!tp) return;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_print_st", 0.0f);


    pr_training_stats_t stats;
    if (pr_training_get_stats(tp, &stats) != 0) return;

    printf("=== Training-Plasticity Statistics ===\n");
    printf("\nStep Counts:\n");
    printf("  Total steps:         %lu\n", (unsigned long)stats.total_steps);
    printf("  Gradient steps:      %lu\n", (unsigned long)stats.gradient_steps);
    printf("  Plasticity steps:    %lu\n", (unsigned long)stats.plasticity_steps);
    printf("  Consolidation steps: %lu\n", (unsigned long)stats.consolidation_steps);

    printf("\nUpdate Statistics:\n");
    printf("  Total weights updated:   %lu\n", (unsigned long)stats.total_weights_updated);
    printf("  Gradient contribution:   %.2f%%\n",
           stats.total_steps > 0 ?
           100.0 * stats.total_gradient_contribution / stats.total_steps : 0.0);
    printf("  Plasticity contribution: %.2f%%\n",
           stats.total_steps > 0 ?
           100.0 * stats.total_plasticity_contribution / stats.total_steps : 0.0);

    printf("\nConversion Statistics:\n");
    printf("  Gradients converted: %lu\n", (unsigned long)stats.gradients_converted);
    printf("  Events converted:    %lu\n", (unsigned long)stats.events_converted);
    printf("  Avg confidence:      %.4f\n", stats.avg_conversion_confidence);

    printf("\nLoss Statistics:\n");
    printf("  Min loss: %.6f\n", stats.min_loss);
    printf("  Max loss: %.6f\n", stats.max_loss);
    printf("  Avg loss: %.6f\n",
           stats.total_steps > 0 ? stats.loss_sum / stats.total_steps : 0.0);

    printf("\nEpoch Statistics:\n");
    printf("  Epochs completed:  %lu\n", (unsigned long)stats.epochs_completed);
    printf("  Total promotions:  %lu\n", (unsigned long)stats.total_promotions);
    printf("  Total demotions:   %lu\n", (unsigned long)stats.total_demotions);

    printf("\nTiming:\n");
    printf("  Total time:      %.2f ms\n", stats.total_training_time_ms);
    printf("  Avg step time:   %.4f ms\n", stats.avg_step_time_ms);

    printf("\nMode Usage:\n");
    for (int m = 0; m < PR_TRAINING_MODE_COUNT; m++) {
        /* Phase 8: Loop progress heartbeat */
        if ((m & 0xFF) == 0 && PR_TRAINING_MODE_COUNT > 256) {
            pr_training_plasticity_heartbeat("pr_training__loop",
                             (float)(m + 1) / (float)PR_TRAINING_MODE_COUNT);
        }

        if (stats.mode_step_counts[m] > 0) {
            printf("  %s: %lu steps\n",
                   pr_training_mode_name((pr_training_mode_t)m),
                   (unsigned long)stats.mode_step_counts[m]);
        }
    }
    printf("======================================\n");
}

void pr_training_print_step_result(const pr_training_step_result_t* result) {
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_print_st", 0.0f);


    printf("Step Result:\n");
    printf("  Phase:             %s\n", pr_training_phase_name(result->phase));
    printf("  Weights updated:   %u\n", result->weights_updated);
    printf("  Total change:      %.6f\n", result->total_weight_change);
    printf("  Gradient contrib:  %.2f%%\n", result->gradient_contribution * 100.0f);
    printf("  Plasticity contrib: %.2f%%\n", result->plasticity_contribution * 100.0f);
    printf("  Loss:              %.6f\n", result->step_loss);
    printf("  Time:              %.4f ms\n", result->step_time_ms);
    if (result->consolidation_triggered) {
        printf("  Consolidation:     TRIGGERED\n");
    }
}

void pr_training_print_consolidation_result(
    const pr_epoch_consolidation_result_t* result)
{
    if (!result) return;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_print_co", 0.0f);


    printf("Epoch Consolidation Result:\n");
    printf("  Memories promoted:    %u\n", result->memories_promoted);
    printf("  Memories demoted:     %u\n", result->memories_demoted);
    printf("  Edges strengthened:   %u\n", result->edges_strengthened);
    printf("  Edges pruned:         %u\n", result->edges_pruned);
    printf("  Time:                 %.2f ms\n", result->consolidation_time_ms);
    printf("  Pre-consol loss:      %.6f\n", result->pre_consolidation_loss);
    printf("  Post-consol loss:     %.6f\n", result->post_consolidation_loss);
}

bool pr_training_validate_model_interface(const pr_model_interface_t* model) {
    if (!model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_validate_model_interface: model is NULL");
        return false;
    }

    /* At minimum need model handle and some operations */
    if (!model->model_handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_validate_model_interface: model->model_handle is NULL");
        return false;
    }

    /* Need at least gradient or loss computation */
    if (!model->get_gradients && !model->compute_loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_validate_model_interface: required parameter is NULL (model->get_gradients, model->compute_loss)");
        return false;
    }

    /* Need apply_updates to do anything useful */
    if (!model->apply_updates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_training_validate_model_interface: model->apply_updates is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_validate", 0.0f);


    return true;
}

size_t pr_training_get_memory_usage(pr_training_plasticity_t tp) {
    if (!tp) return 0;

    /* Phase 8: Heartbeat at operation start */
    pr_training_plasticity_heartbeat("pr_training__pr_training_get_memo", 0.0f);


    size_t usage = sizeof(struct pr_training_plasticity_struct);

    nimcp_mutex_lock(tp->mutex);

    /* STDP buffer */
    usage += tp->stdp_buffer_capacity * sizeof(pr_stdp_timing_t);

    /* Pseudo-gradient buffer */
    usage += tp->pseudo_grad_buffer_capacity * sizeof(pr_pseudo_gradient_t);

    nimcp_mutex_unlock(tp->mutex);

    return usage;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void pr_training_plasticity_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_pr_training_plasticity_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration
 *
 * NOTE: This module already has extensive training functions
 * (pr_training_epoch_start, pr_training_unified_step, pr_training_epoch_end).
 * These wrapper functions delegate to the existing implementations rather
 * than duplicating logic.
 * ============================================================================ */

int pr_training_plasticity_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_training_plasticity_training_begin: NULL argument");
        return -1;
    }
    pr_training_plasticity_heartbeat_instance(NULL, "pr_training_plasticity_training_begin", 0.0f);

    pr_training_plasticity_t tp = (pr_training_plasticity_t)instance;

    /* Delegate to existing epoch_start with epoch 0 for fresh training */
    int rc = pr_training_epoch_start(tp, 0);
    if (rc != 0) return rc;

    /* Also reset cumulative statistics */
    nimcp_mutex_lock(tp->mutex);
    tp->stats.total_steps = 0;
    tp->stats.total_gradient_contribution = 0.0;
    tp->stats.total_plasticity_contribution = 0.0;
    tp->total_steps = 0;
    tp->loss_ema = 0.0f;
    tp->loss_variance_ema = 0.0f;
    tp->last_supervised_loss = 0.0f;
    tp->last_unsupervised_loss = 0.0f;
    nimcp_mutex_unlock(tp->mutex);

    pr_training_plasticity_heartbeat_instance(NULL, "pr_training_plasticity_training_begin", 1.0f);
    return 0;
}

int pr_training_plasticity_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_training_plasticity_training_end: NULL argument");
        return -1;
    }
    pr_training_plasticity_heartbeat_instance(NULL, "pr_training_plasticity_training_end", 0.0f);

    pr_training_plasticity_t tp = (pr_training_plasticity_t)instance;

    /* Delegate to existing epoch_end with consolidation enabled */
    int rc = pr_training_epoch_end(tp, true);

    /* Capture final metrics */
    nimcp_mutex_lock(tp->mutex);
    uint64_t total_steps = tp->total_steps;
    float final_loss_ema = tp->loss_ema;
    (void)total_steps;
    (void)final_loss_ema;
    nimcp_mutex_unlock(tp->mutex);

    pr_training_plasticity_heartbeat_instance(NULL, "pr_training_plasticity_training_end", 1.0f);
    return rc;
}

int pr_training_plasticity_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_training_plasticity_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    pr_training_plasticity_heartbeat_instance(NULL, "pr_training_plasticity_training_step", progress);

    pr_training_plasticity_t tp = (pr_training_plasticity_t)instance;

    /* Adapt learning weights based on training progress */
    nimcp_mutex_lock(tp->mutex);

    /* Adjust supervised/unsupervised balance during training:
     * Early: higher unsupervised weight for exploration
     * Late: higher supervised weight for convergence */
    float supervised_target = 0.3f + 0.4f * progress;   /* 0.3 -> 0.7 */
    float unsupervised_target = 1.0f - supervised_target;

    tp->current_supervised_weight +=
        (supervised_target - tp->current_supervised_weight) * 0.1f;
    tp->current_unsupervised_weight +=
        (unsupervised_target - tp->current_unsupervised_weight) * 0.1f;

    /* Increment step counters */
    tp->total_steps++;
    tp->epoch_step_count++;

    nimcp_mutex_unlock(tp->mutex);

    return 0;
}
