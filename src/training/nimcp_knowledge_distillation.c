/**
 * @file nimcp_knowledge_distillation.c
 * @brief Implementation of Knowledge Distillation for NIMCP Training
 *
 * WHAT: Transfer knowledge from teacher model(s) to student model
 * WHY:  Compress models, improve student accuracy, enable ensemble distillation
 * HOW:  Soft labels, feature matching, attention transfer, relation-based distillation
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 * @version 1.0.0
 */

#include "training/nimcp_knowledge_distillation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(knowledge_distillation)

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Feature matching regressor (for dimension alignment)
 */
typedef struct {
    float* weights;                  /**< Weight matrix */
    float* bias;                     /**< Bias vector */
    uint32_t input_dim;              /**< Input dimension (student) */
    uint32_t output_dim;             /**< Output dimension (teacher) */
} feature_regressor_t;

/**
 * @brief Knowledge distillation context
 */
struct kd_ctx_s {
    kd_config_t config;              /**< Configuration */

    /* Teachers */
    kd_teacher_t teachers[KD_MAX_TEACHERS]; /**< Registered teachers */
    uint32_t num_teachers;           /**< Number of teachers */
    float teacher_weights[KD_MAX_TEACHERS]; /**< Ensemble weights */

    /* Feature matching */
    feature_regressor_t* regressors; /**< Feature regressors */
    uint32_t num_regressors;         /**< Number of regressors */

    /* Cached teacher outputs */
    nimcp_tensor_t* cached_teacher_logits;     /**< Cached teacher logits */
    nimcp_tensor_t** cached_teacher_features;  /**< Cached teacher features */
    uint32_t num_cached_features;              /**< Number of cached features */
    nimcp_tensor_t** cached_teacher_attention; /**< Cached attention maps */
    uint32_t num_cached_attention;             /**< Number of attention maps */

    /* Integration handles */
    void* brain_factory;             /**< Brain factory */
    nimcp_loss_context_t* loss_ctx;  /**< Loss function context */

    /* Statistics */
    kd_stats_t stats;                /**< Runtime statistics */

    /* Thread safety */
    nimcp_mutex_t* mutex;            /**< Mutex for thread safety */
};

//=============================================================================
// Method Names
//=============================================================================

static const char* method_names[] = {
    "Response-Based",
    "Feature-Based",
    "Attention Transfer",
    "Relational",
    "Self-Distillation",
    "Mutual",
    "Ensemble",
    "Progressive",
    "Online",
    "Hybrid"
};

static const char* loss_type_names[] = {
    "KL Divergence",
    "MSE",
    "Cosine",
    "L1",
    "Focal",
    "Pearson"
};

//=============================================================================
// Forward Declarations
//=============================================================================

static float compute_kl_divergence(const float* p, const float* q, size_t count);
static float compute_mse(const float* a, const float* b, size_t count);
static float compute_cosine_loss(const float* a, const float* b, size_t count);
static float compute_l1_loss(const float* a, const float* b, size_t count);
static void softmax(const float* input, float* output, size_t count);
static float log_softmax_at(const float* logits, size_t count, size_t index);

//=============================================================================
// Lifecycle API Implementation
//=============================================================================

int kd_default_config(kd_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "kd_default_config: config is NULL");
        return -1;
    }

    memset(config, 0, sizeof(kd_config_t));

    /* Default: response-based distillation */
    config->method = KD_METHOD_RESPONSE;

    /* Response config (Hinton 2015) */
    config->response.temperature = KD_DEFAULT_TEMPERATURE;
    config->response.alpha = KD_DEFAULT_ALPHA;
    config->response.loss_type = KD_LOSS_KL_DIV;
    config->response.use_teacher_logits = true;

    /* Feature config */
    config->feature.match_method = KD_FEATURE_MATCH_DIRECT;
    config->feature.feature_weight = 0.1f;
    config->feature.normalize_features = true;

    /* Attention config */
    config->attention.attention_weight = 0.1f;
    config->attention.use_spatial_attention = true;
    config->attention.use_channel_attention = false;
    config->attention.p_norm = 2.0f;

    /* Relational config */
    config->relational.use_distance_wise = true;
    config->relational.use_angle_wise = true;
    config->relational.distance_weight = 0.5f;
    config->relational.angle_weight = 0.5f;

    /* Self-distillation config */
    config->self_distill.num_generations = 3;
    config->self_distill.reinitialize_student = true;
    config->self_distill.temperature_decay = NIMCP_EMA_DECAY_FAST;

    /* Mutual config */
    config->mutual.num_peers = 2;
    config->mutual.symmetric_kl = true;
    config->mutual.mutual_weight = 0.5f;

    /* Ensemble config */
    config->ensemble.weighting = KD_ENSEMBLE_UNIFORM;
    config->ensemble.uncertainty_weighting = false;

    /* General settings */
    config->hard_label_weight = 1.0f - KD_DEFAULT_ALPHA;
    config->teacher_eval_mode = true;
    config->detach_teacher = true;

    /* Integration */
    config->integrate_loss_functions = true;
    config->integrate_brain_factory = false;

    /* Debugging */
    config->verbose = false;
    config->track_statistics = true;

    return 0;
}

kd_ctx_t* kd_create(const kd_config_t* config) {
    if (!config) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "kd_create: config is NULL");
        return NULL;
    }

    /* Validate configuration */
    if (kd_validate_config(config) != 0) {
        NIMCP_THROW(NIMCP_ERROR_CONFIG_INVALID, "kd_create: config validation failed");
        return NULL;
    }

    kd_ctx_t* ctx = nimcp_calloc(1, sizeof(kd_ctx_t));
    if (!ctx) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(kd_ctx_t),
                          "kd_create: failed to allocate context");
        return NULL;
    }

    /* Copy configuration */
    memcpy(&ctx->config, config, sizeof(kd_config_t));

    /* Create mutex */
    mutex_attr_t attr = {0};
    attr.type = MUTEX_TYPE_NORMAL;
    ctx->mutex = nimcp_mutex_create(&attr);
    if (!ctx->mutex) {
        NIMCP_THROW_THREADING(NIMCP_ERROR_MUTEX_INIT, 0,
                             "kd_create: failed to create mutex");
        nimcp_free(ctx);
        return NULL;
    }

    /* Initialize teacher weights to uniform */
    for (uint32_t i = 0; i < KD_MAX_TEACHERS; i++) {
        ctx->teacher_weights[i] = 1.0f;
    }

    ctx->num_teachers = 0;

    /* Initialize feature regressors if needed */
    if (config->method == KD_METHOD_FEATURE ||
        config->method == KD_METHOD_HYBRID) {
        if (config->feature.match_method == KD_FEATURE_MATCH_REGRESSOR) {
            ctx->regressors = nimcp_calloc(KD_MAX_FEATURE_LAYERS, sizeof(feature_regressor_t));
        }
    }

    /* Reset statistics */
    memset(&ctx->stats, 0, sizeof(kd_stats_t));

    if (config->verbose) {
        printf("[KD] Created context: method=%s, T=%.1f, alpha=%.2f\n",
               kd_method_name(config->method),
               config->response.temperature,
               config->response.alpha);
    }

    return ctx;
}

void kd_destroy(kd_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    /* Clean up feature regressors */
    if (ctx->regressors) {
        for (uint32_t i = 0; i < ctx->num_regressors; i++) {
            if (ctx->regressors[i].weights) {
                nimcp_free(ctx->regressors[i].weights);
            }
            if (ctx->regressors[i].bias) {
                nimcp_free(ctx->regressors[i].bias);
            }
        }
        nimcp_free(ctx->regressors);
    }

    /* Clean up cached outputs */
    if (ctx->cached_teacher_logits) {
        nimcp_tensor_destroy(ctx->cached_teacher_logits);
    }

    if (ctx->cached_teacher_features) {
        for (uint32_t i = 0; i < ctx->num_cached_features; i++) {
            if (ctx->cached_teacher_features[i]) {
                nimcp_tensor_destroy(ctx->cached_teacher_features[i]);
            }
        }
        nimcp_free(ctx->cached_teacher_features);
    }

    if (ctx->cached_teacher_attention) {
        for (uint32_t i = 0; i < ctx->num_cached_attention; i++) {
            if (ctx->cached_teacher_attention[i]) {
                nimcp_tensor_destroy(ctx->cached_teacher_attention[i]);
            }
        }
        nimcp_free(ctx->cached_teacher_attention);
    }

    /* Destroy mutex */
    if (ctx->mutex) {
        nimcp_mutex_free(ctx->mutex);
    }

    nimcp_free(ctx);
}

//=============================================================================
// Teacher Management Implementation
//=============================================================================

int kd_register_teacher(kd_ctx_t* ctx, const kd_teacher_t* teacher) {
    if (!ctx || !teacher || !teacher->model || !teacher->forward) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kd_register_teacher: required parameter is NULL (ctx, teacher, teacher->model, teacher->forward)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    if (ctx->num_teachers >= KD_MAX_TEACHERS) {
        nimcp_mutex_unlock(ctx->mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kd_register_teacher: capacity exceeded");
        return -1;
    }

    uint32_t idx = ctx->num_teachers;
    memcpy(&ctx->teachers[idx], teacher, sizeof(kd_teacher_t));

    /* Default: force eval mode for teachers (prevents dropout, batchnorm training) */
    ctx->teachers[idx].force_eval_mode = true;

    ctx->num_teachers++;

    /* Update weights for uniform distribution */
    if (ctx->config.ensemble.weighting == KD_ENSEMBLE_UNIFORM) {
        float uniform_weight = 1.0f / (float)ctx->num_teachers;
        for (uint32_t i = 0; i < ctx->num_teachers; i++) {
            ctx->teacher_weights[i] = uniform_weight;
        }
    }

    if (ctx->config.verbose) {
        printf("[KD] Registered teacher '%s' at index %u\n",
               teacher->name ? teacher->name : "unnamed", idx);
    }

    nimcp_mutex_unlock(ctx->mutex);
    return (int)idx;
}

int kd_unregister_teacher(kd_ctx_t* ctx, uint32_t teacher_idx) {
    if (!ctx || teacher_idx >= ctx->num_teachers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kd_unregister_teacher: ctx is NULL");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Shift remaining teachers */
    for (uint32_t i = teacher_idx; i < ctx->num_teachers - 1; i++) {
        memcpy(&ctx->teachers[i], &ctx->teachers[i + 1], sizeof(kd_teacher_t));
        ctx->teacher_weights[i] = ctx->teacher_weights[i + 1];
    }
    ctx->num_teachers--;

    /* Renormalize weights */
    if (ctx->config.ensemble.weighting == KD_ENSEMBLE_UNIFORM && ctx->num_teachers > 0) {
        float uniform_weight = 1.0f / (float)ctx->num_teachers;
        for (uint32_t i = 0; i < ctx->num_teachers; i++) {
            ctx->teacher_weights[i] = uniform_weight;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

uint32_t kd_get_teacher_count(const kd_ctx_t* ctx) {
    if (!ctx) {
        return 0;
    }
    return ctx->num_teachers;
}

int kd_set_teacher_weights(kd_ctx_t* ctx, const float* weights) {
    if (!ctx || !weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kd_set_teacher_weights: required parameter is NULL (ctx, weights)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);

    /* Copy and normalize weights */
    float sum = 0.0f;
    for (uint32_t i = 0; i < ctx->num_teachers; i++) {
        ctx->teacher_weights[i] = weights[i];
        sum += weights[i];
    }

    /* Normalize to sum to 1 */
    if (sum > 0.0f) {
        for (uint32_t i = 0; i < ctx->num_teachers; i++) {
            ctx->teacher_weights[i] /= sum;
        }
    }

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

//=============================================================================
// Distillation Loss Implementation
//=============================================================================

int kd_compute_loss(
    kd_ctx_t* ctx,
    const nimcp_tensor_t* student_logits,
    const nimcp_tensor_t* input,
    const nimcp_tensor_t* targets,
    float* loss
) {
    if (!ctx || !student_logits || !input || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kd_compute_loss: required parameter is NULL (ctx, student_logits, input, loss)");
        return -1;
    }

    if (ctx->num_teachers == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "kd_compute_loss: ctx->num_teachers is zero");
        return -1;  /* No teachers registered */
    }

    nimcp_mutex_lock(ctx->mutex);

    float total_soft_loss = 0.0f;
    float total_hard_loss = 0.0f;

    size_t student_count = nimcp_tensor_numel(student_logits);
    const float* student_data = nimcp_tensor_data((nimcp_tensor_t*)student_logits);

    /* Forward through each teacher and compute soft loss */
    for (uint32_t t = 0; t < ctx->num_teachers; t++) {
        kd_teacher_t* teacher = &ctx->teachers[t];

        /* CRITICAL: Enforce teacher evaluation mode during distillation
         * Teacher models should not use dropout, batch norm in training mode, etc.
         * This ensures consistent, stable soft labels for distillation. */
        bool was_eval_mode = true;
        if (teacher->set_eval_mode && teacher->is_eval_mode) {
            was_eval_mode = teacher->is_eval_mode(teacher->model);
            if (!was_eval_mode || teacher->force_eval_mode) {
                teacher->set_eval_mode(teacher->model, true);
            }
        }

        /* Allocate output tensor for teacher */
        nimcp_tensor_t* teacher_output = nimcp_tensor_clone(student_logits);
        if (!teacher_output) {
            nimcp_mutex_unlock(ctx->mutex);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "kd_compute_loss: teacher_output is NULL");
            return -1;
        }

        /* Forward through teacher (in eval mode) */
        nimcp_tensor_t** features = NULL;
        uint32_t num_features = 0;
        int result = teacher->forward(teacher->model, input, teacher_output, features, &num_features);

        /* Restore original mode if needed and if force_eval is not set */
        if (teacher->set_eval_mode && !teacher->force_eval_mode && !was_eval_mode) {
            teacher->set_eval_mode(teacher->model, false);
        }

        if (result != 0) {
            nimcp_tensor_destroy(teacher_output);
            nimcp_mutex_unlock(ctx->mutex);
            return result;
        }

        ctx->stats.teacher_forward_count++;

        /* Compute response-based loss */
        const float* teacher_data = nimcp_tensor_data(teacher_output);
        float soft_loss = kd_response_loss(ctx, student_logits, teacher_output,
                                           ctx->config.response.temperature);

        total_soft_loss += ctx->teacher_weights[t] * soft_loss;

        /* Clean up teacher features */
        if (features) {
            for (uint32_t f = 0; f < num_features; f++) {
                if (features[f]) {
                    nimcp_tensor_destroy(features[f]);
                }
            }
            nimcp_free(features);
        }

        nimcp_tensor_destroy(teacher_output);
    }

    /* Compute hard label loss if targets provided */
    if (targets) {
        size_t target_count = nimcp_tensor_numel(targets);
        const float* target_data = nimcp_tensor_data((nimcp_tensor_t*)targets);

        /* Cross-entropy loss for hard labels */
        if (student_count > 0 && target_count > 0) {
            /* Assuming targets are one-hot or class indices */
            /* Compute cross-entropy: -sum(target * log(softmax(logits))) */
            size_t batch_size = target_count;
            size_t num_classes = student_count / batch_size;

            if (num_classes > 0) {
                for (size_t b = 0; b < batch_size; b++) {
                    size_t target_class = (size_t)target_data[b];
                    if (target_class < num_classes) {
                        total_hard_loss -= log_softmax_at(
                            student_data + b * num_classes,
                            num_classes,
                            target_class);
                    }
                }
                total_hard_loss /= (float)batch_size;
            }
        }
    }

    /* Combine losses */
    float alpha = ctx->config.response.alpha;
    *loss = alpha * total_soft_loss + (1.0f - alpha) * total_hard_loss;

    /* Update statistics */
    ctx->stats.total_steps++;
    ctx->stats.total_soft_loss += total_soft_loss;
    ctx->stats.total_hard_loss += total_hard_loss;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

int kd_compute_loss_with_features(
    kd_ctx_t* ctx,
    const nimcp_tensor_t* student_logits,
    nimcp_tensor_t** student_features,
    uint32_t num_student_features,
    const nimcp_tensor_t* input,
    const nimcp_tensor_t* targets,
    float* loss
) {
    if (!ctx || !student_logits || !input || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kd_compute_loss_with_features: required parameter is NULL (ctx, student_logits, input, loss)");
        return -1;
    }

    /* First compute standard loss */
    float base_loss = 0.0f;
    int result = kd_compute_loss(ctx, student_logits, input, targets, &base_loss);
    if (result != 0) {
        return result;
    }

    nimcp_mutex_lock(ctx->mutex);

    float feature_loss = 0.0f;

    /* Forward through each teacher and compute feature loss */
    if (student_features && num_student_features > 0) {
        for (uint32_t t = 0; t < ctx->num_teachers; t++) {
            kd_teacher_t* teacher = &ctx->teachers[t];

            /* Forward through teacher to get features */
            nimcp_tensor_t* teacher_output = nimcp_tensor_clone(student_logits);
            if (!teacher_output) {
                nimcp_mutex_unlock(ctx->mutex);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kd_compute_loss_with_features: teacher_output is NULL");
                return -1;
            }

            nimcp_tensor_t** teacher_features = NULL;
            uint32_t num_teacher_features = 0;
            result = teacher->forward(teacher->model, input, teacher_output,
                                     teacher_features, &num_teacher_features);
            if (result != 0) {
                nimcp_tensor_destroy(teacher_output);
                nimcp_mutex_unlock(ctx->mutex);
                return result;
            }

            /* Compute feature matching loss */
            if (teacher_features && num_teacher_features > 0) {
                uint32_t num_pairs = (num_student_features < num_teacher_features) ?
                                     num_student_features : num_teacher_features;
                float feat_loss = kd_feature_loss(ctx, student_features,
                                                  teacher_features, num_pairs);
                feature_loss += ctx->teacher_weights[t] * feat_loss;

                /* Clean up teacher features */
                for (uint32_t f = 0; f < num_teacher_features; f++) {
                    if (teacher_features[f]) {
                        nimcp_tensor_destroy(teacher_features[f]);
                    }
                }
                nimcp_free(teacher_features);
            }

            nimcp_tensor_destroy(teacher_output);
        }
    }

    *loss = base_loss + ctx->config.feature.feature_weight * feature_loss;

    /* Update statistics */
    ctx->stats.total_feature_loss += feature_loss;

    nimcp_mutex_unlock(ctx->mutex);
    return 0;
}

float kd_response_loss(
    kd_ctx_t* ctx,
    const nimcp_tensor_t* student_logits,
    const nimcp_tensor_t* teacher_logits,
    float temperature
) {
    if (!ctx || !student_logits || !teacher_logits) {
        return 0.0f;
    }

    size_t count = nimcp_tensor_numel(student_logits);
    size_t teacher_count = nimcp_tensor_numel(teacher_logits);
    if (count == 0 || count != teacher_count) {
        return 0.0f;
    }

    const float* student_data = nimcp_tensor_data((nimcp_tensor_t*)student_logits);
    const float* teacher_data = nimcp_tensor_data((nimcp_tensor_t*)teacher_logits);

    if (!student_data || !teacher_data) {
        return 0.0f;
    }

    /* Allocate temporary buffers for softmax outputs */
    float* student_soft = nimcp_calloc(count, sizeof(float));
    float* teacher_soft = nimcp_calloc(count, sizeof(float));
    if (!student_soft || !teacher_soft) {
        if (student_soft) nimcp_free(student_soft);
        if (teacher_soft) nimcp_free(teacher_soft);
        return 0.0f;
    }

    /* Scale logits by temperature */
    float inv_temp = 1.0f / temperature;
    for (size_t i = 0; i < count; i++) {
        student_soft[i] = student_data[i] * inv_temp;
        teacher_soft[i] = teacher_data[i] * inv_temp;
    }

    /* Apply softmax */
    softmax(student_soft, student_soft, count);
    softmax(teacher_soft, teacher_soft, count);

    /* Compute loss based on type */
    float loss = 0.0f;
    switch (ctx->config.response.loss_type) {
        case KD_LOSS_KL_DIV:
            loss = compute_kl_divergence(teacher_soft, student_soft, count);
            break;
        case KD_LOSS_MSE:
            loss = compute_mse(student_soft, teacher_soft, count);
            break;
        case KD_LOSS_COSINE:
            loss = compute_cosine_loss(student_soft, teacher_soft, count);
            break;
        case KD_LOSS_L1:
            loss = compute_l1_loss(student_soft, teacher_soft, count);
            break;
        default:
            loss = compute_kl_divergence(teacher_soft, student_soft, count);
            break;
    }

    /* Scale by T^2 to maintain gradient scale (Hinton 2015) */
    loss *= temperature * temperature;

    nimcp_free(student_soft);
    nimcp_free(teacher_soft);

    return loss;
}

float kd_feature_loss(
    kd_ctx_t* ctx,
    nimcp_tensor_t** student_features,
    nimcp_tensor_t** teacher_features,
    uint32_t num_features
) {
    if (!ctx || !student_features || !teacher_features || num_features == 0) {
        return 0.0f;
    }

    float total_loss = 0.0f;

    for (uint32_t i = 0; i < num_features; i++) {
        if (!student_features[i] || !teacher_features[i]) {
            continue;
        }

        size_t student_count = nimcp_tensor_numel(student_features[i]);
        size_t teacher_count = nimcp_tensor_numel(teacher_features[i]);

        const float* student_data = nimcp_tensor_data(student_features[i]);
        const float* teacher_data = nimcp_tensor_data(teacher_features[i]);

        if (!student_data || !teacher_data || student_count == 0 || teacher_count == 0) {
            continue;
        }

        float layer_loss = 0.0f;

        switch (ctx->config.feature.match_method) {
            case KD_FEATURE_MATCH_DIRECT:
                if (student_count == teacher_count) {
                    if (ctx->config.feature.normalize_features) {
                        /* Normalize then compare */
                        float student_norm = 0.0f, teacher_norm = 0.0f;
                        for (size_t j = 0; j < student_count; j++) {
                            student_norm += student_data[j] * student_data[j];
                            teacher_norm += teacher_data[j] * teacher_data[j];
                        }
                        student_norm = sqrtf(student_norm + 1e-8f);
                        teacher_norm = sqrtf(teacher_norm + 1e-8f);

                        for (size_t j = 0; j < student_count; j++) {
                            float s = student_data[j] / student_norm;
                            float t = teacher_data[j] / teacher_norm;
                            layer_loss += (s - t) * (s - t);
                        }
                        layer_loss /= (float)student_count;
                    } else {
                        layer_loss = compute_mse(student_data, teacher_data, student_count);
                    }
                }
                break;

            case KD_FEATURE_MATCH_GRAM:
                /* Gram matrix matching (style loss) */
                /* Simplified: just compute MSE for now */
                if (student_count == teacher_count) {
                    layer_loss = compute_mse(student_data, teacher_data, student_count);
                }
                break;

            default:
                if (student_count == teacher_count) {
                    layer_loss = compute_mse(student_data, teacher_data, student_count);
                }
                break;
        }

        total_loss += layer_loss;
    }

    return total_loss / (float)num_features;
}

float kd_attention_loss(
    kd_ctx_t* ctx,
    nimcp_tensor_t** student_attention,
    nimcp_tensor_t** teacher_attention,
    uint32_t num_layers
) {
    if (!ctx || !student_attention || !teacher_attention || num_layers == 0) {
        return 0.0f;
    }

    float total_loss = 0.0f;
    float p = ctx->config.attention.p_norm;

    for (uint32_t i = 0; i < num_layers; i++) {
        if (!student_attention[i] || !teacher_attention[i]) {
            continue;
        }

        size_t student_count = nimcp_tensor_numel(student_attention[i]);
        size_t teacher_count = nimcp_tensor_numel(teacher_attention[i]);

        if (student_count != teacher_count || student_count == 0) {
            continue;
        }

        const float* student_data = nimcp_tensor_data(student_attention[i]);
        const float* teacher_data = nimcp_tensor_data(teacher_attention[i]);

        if (!student_data || !teacher_data) {
            continue;
        }

        /* Compute attention map norm */
        float student_norm = 0.0f, teacher_norm = 0.0f;
        for (size_t j = 0; j < student_count; j++) {
            student_norm += powf(fabsf(student_data[j]), p);
            teacher_norm += powf(fabsf(teacher_data[j]), p);
        }
        student_norm = powf(student_norm, 1.0f / p);
        teacher_norm = powf(teacher_norm, 1.0f / p);

        /* Normalize and compute loss */
        float layer_loss = 0.0f;
        for (size_t j = 0; j < student_count; j++) {
            float s = (student_norm > 1e-8f) ? student_data[j] / student_norm : student_data[j];
            float t = (teacher_norm > 1e-8f) ? teacher_data[j] / teacher_norm : teacher_data[j];
            layer_loss += powf(fabsf(s - t), p);
        }
        layer_loss = powf(layer_loss, 1.0f / p);

        total_loss += layer_loss;
    }

    return total_loss / (float)num_layers;
}

float kd_relational_loss(
    kd_ctx_t* ctx,
    const nimcp_tensor_t* student_embeddings,
    const nimcp_tensor_t* teacher_embeddings
) {
    if (!ctx || !student_embeddings || !teacher_embeddings) {
        return 0.0f;
    }

    /* Assuming embeddings are [batch_size, embedding_dim] */
    size_t student_count = nimcp_tensor_numel(student_embeddings);
    size_t teacher_count = nimcp_tensor_numel(teacher_embeddings);

    if (student_count != teacher_count || student_count == 0) {
        return 0.0f;
    }

    const float* student_data = nimcp_tensor_data((nimcp_tensor_t*)student_embeddings);
    const float* teacher_data = nimcp_tensor_data((nimcp_tensor_t*)teacher_embeddings);

    if (!student_data || !teacher_data) {
        return 0.0f;
    }

    /* Get dimensions (simplified: assume batch_size * embedding_dim) */
    /* For proper implementation, would need tensor shape info */
    size_t total_count = student_count;

    float distance_loss = 0.0f;
    float angle_loss = 0.0f;

    if (ctx->config.relational.use_distance_wise) {
        /* Distance-wise loss: match pairwise distances */
        /* Simplified: compute MSE between normalized embeddings */
        float student_norm = 0.0f, teacher_norm = 0.0f;
        for (size_t i = 0; i < total_count; i++) {
            student_norm += student_data[i] * student_data[i];
            teacher_norm += teacher_data[i] * teacher_data[i];
        }
        student_norm = sqrtf(student_norm + 1e-8f);
        teacher_norm = sqrtf(teacher_norm + 1e-8f);

        for (size_t i = 0; i < total_count; i++) {
            float s = student_data[i] / student_norm;
            float t = teacher_data[i] / teacher_norm;
            distance_loss += (s - t) * (s - t);
        }
        distance_loss /= (float)total_count;
    }

    if (ctx->config.relational.use_angle_wise) {
        /* Angle-wise loss: match pairwise angles (cosine similarity) */
        float dot = 0.0f, s_mag = 0.0f, t_mag = 0.0f;
        for (size_t i = 0; i < total_count; i++) {
            dot += student_data[i] * teacher_data[i];
            s_mag += student_data[i] * student_data[i];
            t_mag += teacher_data[i] * teacher_data[i];
        }
        s_mag = sqrtf(s_mag + 1e-8f);
        t_mag = sqrtf(t_mag + 1e-8f);
        float cosine = dot / (s_mag * t_mag);
        angle_loss = 1.0f - cosine;  /* Cosine similarity loss */
    }

    return ctx->config.relational.distance_weight * distance_loss +
           ctx->config.relational.angle_weight * angle_loss;
}

//=============================================================================
// Integration API Implementation
//=============================================================================

int kd_connect_brain_factory(kd_ctx_t* ctx, void* brain_factory) {
    if (!ctx || !brain_factory) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kd_connect_brain_factory: required parameter is NULL (ctx, brain_factory)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->brain_factory = brain_factory;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

int kd_connect_loss_functions(kd_ctx_t* ctx, nimcp_loss_context_t* loss_ctx) {
    if (!ctx || !loss_ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kd_connect_loss_functions: required parameter is NULL (ctx, loss_ctx)");
        return -1;
    }

    nimcp_mutex_lock(ctx->mutex);
    ctx->loss_ctx = loss_ctx;
    nimcp_mutex_unlock(ctx->mutex);

    return 0;
}

//=============================================================================
// Statistics API Implementation
//=============================================================================

int kd_get_stats(const kd_ctx_t* ctx, kd_stats_t* stats) {
    if (!ctx || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "kd_get_stats: required parameter is NULL (ctx, stats)");
        return -1;
    }

    nimcp_mutex_lock((nimcp_mutex_t*)ctx->mutex);
    memcpy(stats, &ctx->stats, sizeof(kd_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)ctx->mutex);

    return 0;
}

void kd_reset_stats(kd_ctx_t* ctx) {
    if (!ctx) {
        return;
    }

    nimcp_mutex_lock(ctx->mutex);
    memset(&ctx->stats, 0, sizeof(kd_stats_t));
    nimcp_mutex_unlock(ctx->mutex);
}

//=============================================================================
// Utility Functions Implementation
//=============================================================================

const char* kd_method_name(kd_method_t method) {
    if (method >= KD_METHOD_COUNT) {
        return "Unknown";
    }
    return method_names[method];
}

const char* kd_loss_type_name(kd_loss_type_t type) {
    if (type >= KD_LOSS_COUNT) {
        return "Unknown";
    }
    return loss_type_names[type];
}

int kd_validate_config(const kd_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Validate method */
    if (config->method >= KD_METHOD_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kd_validate_config: capacity exceeded");
        return -1;
    }

    /* Validate temperature */
    if (config->response.temperature <= 0.0f) {
        return -1;
    }

    /* Validate alpha */
    if (config->response.alpha < 0.0f || config->response.alpha > 1.0f) {
        return -1;
    }

    /* Validate loss type */
    if (config->response.loss_type >= KD_LOSS_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "kd_validate_config: capacity exceeded");
        return -1;
    }

    return 0;
}

void kd_softmax_temperature(
    const float* logits,
    float* output,
    size_t count,
    float temperature
) {
    if (!logits || !output || count == 0 || temperature <= 0.0f) {
        return;
    }

    /* Scale by temperature */
    float inv_temp = 1.0f / temperature;
    for (size_t i = 0; i < count; i++) {
        output[i] = logits[i] * inv_temp;
    }

    /* Apply softmax */
    softmax(output, output, count);
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Compute KL divergence: sum(p * log(p/q))
 */
static float compute_kl_divergence(const float* p, const float* q, size_t count) {
    float kl = 0.0f;
    for (size_t i = 0; i < count; i++) {
        if (p[i] > 1e-10f && q[i] > 1e-10f) {
            kl += p[i] * logf(p[i] / q[i]);
        }
    }
    return kl;
}

/**
 * @brief Compute mean squared error
 */
static float compute_mse(const float* a, const float* b, size_t count) {
    float mse = 0.0f;
    for (size_t i = 0; i < count; i++) {
        float diff = a[i] - b[i];
        mse += diff * diff;
    }
    return mse / (float)count;
}

/**
 * @brief Compute cosine similarity loss (1 - cosine_similarity)
 */
static float compute_cosine_loss(const float* a, const float* b, size_t count) {
    float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
    for (size_t i = 0; i < count; i++) {
        dot += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }
    mag_a = sqrtf(mag_a + 1e-8f);
    mag_b = sqrtf(mag_b + 1e-8f);
    float cosine = dot / (mag_a * mag_b);
    return 1.0f - cosine;
}

/**
 * @brief Compute L1/MAE loss
 */
static float compute_l1_loss(const float* a, const float* b, size_t count) {
    float l1 = 0.0f;
    for (size_t i = 0; i < count; i++) {
        l1 += fabsf(a[i] - b[i]);
    }
    return l1 / (float)count;
}

/**
 * @brief Compute softmax in-place
 */
static void softmax(const float* input, float* output, size_t count) {
    /* Find max for numerical stability */
    float max_val = -FLT_MAX;
    for (size_t i = 0; i < count; i++) {
        if (input[i] > max_val) {
            max_val = input[i];
        }
    }

    /* Compute exp and sum */
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }

    /* Normalize */
    for (size_t i = 0; i < count; i++) {
        output[i] /= sum;
    }
}

/**
 * @brief Compute log softmax at specific index
 */
static float log_softmax_at(const float* logits, size_t count, size_t index) {
    if (index >= count) {
        return 0.0f;
    }

    /* log_softmax[i] = logits[i] - log(sum(exp(logits))) */
    float max_val = -FLT_MAX;
    for (size_t i = 0; i < count; i++) {
        if (logits[i] > max_val) {
            max_val = logits[i];
        }
    }

    float sum_exp = 0.0f;
    for (size_t i = 0; i < count; i++) {
        sum_exp += expf(logits[i] - max_val);
    }

    return (logits[index] - max_val) - logf(sum_exp);
}
