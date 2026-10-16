/**
 * @file nimcp_gpu_stubs_training.c
 * @brief CPU fallback implementations for GPU training functions
 *
 * WHAT: Provides functional CPU fallback implementations for all training GPU functions
 * WHY:  Allows building and running without CUDA - enables testing on CPU-only systems
 * HOW:  Implements equivalent CPU algorithms for loss, gradient, optimizer, backward ops
 *
 * All 31 functions from gpu/training/nimcp_training_gpu.h are implemented here with
 * real computation (not just stubs returning errors).
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include "gpu/training/nimcp_training_gpu.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <float.h>

/*=============================================================================
 * Helper: get float pointer from tensor data (assumes FP32 precision)
 *=============================================================================*/

static inline float* tensor_f32(const nimcp_gpu_tensor_t* t) {
    return (float*)t->data;
}

/*=============================================================================
 * Loss Functions (6)
 *=============================================================================*/

/* 1. MSE Loss: mean((pred - target)^2) */
bool nimcp_gpu_loss_mse(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad)
{
    (void)ctx;

    if (!pred || !target || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_loss_mse: required parameter is NULL");
        return false;
    }
    if (pred->numel != target->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_loss_mse: pred and target size mismatch");
        return false;
    }

    const float* p = tensor_f32(pred);
    const float* t = tensor_f32(target);
    size_t n = pred->numel;
    double sum = 0.0;

    for (size_t i = 0; i < n; i++) {
        double diff = (double)p[i] - (double)t[i];
        sum += diff * diff;
    }
    *loss = (float)(sum / (double)n);

    if (grad) {
        if (grad->numel != n) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "nimcp_gpu_loss_mse: grad size mismatch");
            return false;
        }
        float* g = tensor_f32(grad);
        float scale = 2.0f / (float)n;
        for (size_t i = 0; i < n; i++) {
            g[i] = scale * (p[i] - t[i]);
        }
    }

    return true;
}

/* 2. MAE Loss: mean(|pred - target|) */
bool nimcp_gpu_loss_mae(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad)
{
    (void)ctx;

    if (!pred || !target || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_loss_mae: required parameter is NULL");
        return false;
    }
    if (pred->numel != target->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_loss_mae: pred and target size mismatch");
        return false;
    }

    const float* p = tensor_f32(pred);
    const float* t = tensor_f32(target);
    size_t n = pred->numel;
    double sum = 0.0;

    for (size_t i = 0; i < n; i++) {
        sum += fabs((double)p[i] - (double)t[i]);
    }
    *loss = (float)(sum / (double)n);

    if (grad) {
        if (grad->numel != n) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "nimcp_gpu_loss_mae: grad size mismatch");
            return false;
        }
        float* g = tensor_f32(grad);
        float inv_n = 1.0f / (float)n;
        for (size_t i = 0; i < n; i++) {
            float diff = p[i] - t[i];
            if (diff > 0.0f) {
                g[i] = inv_n;
            } else if (diff < 0.0f) {
                g[i] = -inv_n;
            } else {
                g[i] = 0.0f;
            }
        }
    }

    return true;
}

/* 3. Cross Entropy Loss: -sum(target * log(softmax(logits))) */
bool nimcp_gpu_loss_cross_entropy(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* logits,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad,
    int reduction)
{
    (void)ctx;

    if (!logits || !target || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_loss_cross_entropy: required parameter is NULL");
        return false;
    }

    const float* x = tensor_f32(logits);
    const float* t = tensor_f32(target);

    /*
     * Treat as 2D: (batch_size, num_classes)
     * If 1D, batch_size = 1, num_classes = numel
     * If 2D, batch_size = dims[0], num_classes = dims[1]
     */
    size_t batch_size = 1;
    size_t num_classes = logits->numel;
    if (logits->ndim >= 2) {
        batch_size = logits->dims[0];
        num_classes = logits->dims[logits->ndim - 1];
    }

    /* Allocate softmax buffer */
    float* softmax_buf = nimcp_malloc(logits->numel * sizeof(float));
    if (!softmax_buf) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_gpu_loss_cross_entropy: softmax_buf allocation failed");
        return false;
    }

    double total_loss = 0.0;

    for (size_t b = 0; b < batch_size; b++) {
        const float* logits_row = x + b * num_classes;
        const float* target_row = t + b * num_classes;
        float* sm_row = softmax_buf + b * num_classes;

        /* Numerically stable softmax: subtract max */
        float max_val = logits_row[0];
        for (size_t c = 1; c < num_classes; c++) {
            if (logits_row[c] > max_val) max_val = logits_row[c];
        }

        double sum_exp = 0.0;
        for (size_t c = 0; c < num_classes; c++) {
            sm_row[c] = expf(logits_row[c] - max_val);
            sum_exp += (double)sm_row[c];
        }
        for (size_t c = 0; c < num_classes; c++) {
            sm_row[c] = (float)((double)sm_row[c] / sum_exp);
        }

        /* Cross entropy: -sum(target * log(softmax)) */
        double row_loss = 0.0;
        for (size_t c = 0; c < num_classes; c++) {
            if (target_row[c] > 0.0f) {
                float safe_sm = sm_row[c] > 1e-7f ? sm_row[c] : 1e-7f;
                row_loss -= (double)target_row[c] * log((double)safe_sm);
            }
        }
        total_loss += row_loss;
    }

    /* Apply reduction */
    if (reduction == 1) {
        /* mean */
        *loss = (float)(total_loss / (double)batch_size);
    } else if (reduction == 2) {
        /* sum */
        *loss = (float)total_loss;
    } else {
        /* none - store total (best we can do with a scalar output) */
        *loss = (float)total_loss;
    }

    /* Gradient: softmax(logits) - target (for mean reduction, divide by batch) */
    if (grad) {
        if (grad->numel != logits->numel) {
            nimcp_free(softmax_buf);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "nimcp_gpu_loss_cross_entropy: grad size mismatch");
            return false;
        }
        float* g = tensor_f32(grad);
        float scale = 1.0f;
        if (reduction == 1) {
            scale = 1.0f / (float)batch_size;
        }

        for (size_t i = 0; i < logits->numel; i++) {
            g[i] = (softmax_buf[i] - t[i]) * scale;
        }
    }

    nimcp_free(softmax_buf);
    return true;
}

/* 4. Binary Cross Entropy: -[t*log(p) + (1-t)*log(1-p)] */
bool nimcp_gpu_loss_bce(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad)
{
    (void)ctx;

    if (!pred || !target || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_loss_bce: required parameter is NULL");
        return false;
    }
    if (pred->numel != target->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_loss_bce: pred and target size mismatch");
        return false;
    }

    const float* p = tensor_f32(pred);
    const float* t = tensor_f32(target);
    size_t n = pred->numel;
    double sum = 0.0;

    /* Clamp predictions to avoid log(0) */
    const float eps = 1e-7f;

    for (size_t i = 0; i < n; i++) {
        float p_clamped = p[i] < eps ? eps : (p[i] > (1.0f - eps) ? (1.0f - eps) : p[i]);
        sum -= (double)t[i] * log((double)p_clamped) +
               (1.0 - (double)t[i]) * log(1.0 - (double)p_clamped);
    }
    *loss = (float)(sum / (double)n);

    if (grad) {
        if (grad->numel != n) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "nimcp_gpu_loss_bce: grad size mismatch");
            return false;
        }
        float* g = tensor_f32(grad);
        float inv_n = 1.0f / (float)n;
        for (size_t i = 0; i < n; i++) {
            float p_clamped = p[i] < eps ? eps : (p[i] > (1.0f - eps) ? (1.0f - eps) : p[i]);
            /* d/dp BCE = (-t/p + (1-t)/(1-p)) / n */
            g[i] = inv_n * (-t[i] / p_clamped + (1.0f - t[i]) / (1.0f - p_clamped));
        }
    }

    return true;
}

/* 5. Focal Loss: -alpha * (1 - p_t)^gamma * log(p_t) */
bool nimcp_gpu_loss_focal(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad,
    float alpha,
    float gamma)
{
    (void)ctx;

    if (!pred || !target || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_loss_focal: required parameter is NULL");
        return false;
    }
    if (pred->numel != target->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_loss_focal: pred and target size mismatch");
        return false;
    }

    const float* p = tensor_f32(pred);
    const float* t = tensor_f32(target);
    size_t n = pred->numel;
    const float eps = 1e-7f;
    double sum = 0.0;

    for (size_t i = 0; i < n; i++) {
        float p_clamped = p[i] < eps ? eps : (p[i] > (1.0f - eps) ? (1.0f - eps) : p[i]);
        /* p_t = t*p + (1-t)*(1-p) */
        float p_t = t[i] * p_clamped + (1.0f - t[i]) * (1.0f - p_clamped);
        float focal_weight = powf(1.0f - p_t, gamma);
        sum -= (double)(alpha * focal_weight * logf(p_t > eps ? p_t : eps));
    }
    *loss = (float)(sum / (double)n);

    if (grad) {
        if (grad->numel != n) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "nimcp_gpu_loss_focal: grad size mismatch");
            return false;
        }
        float* g = tensor_f32(grad);
        float inv_n = 1.0f / (float)n;
        for (size_t i = 0; i < n; i++) {
            float p_clamped = p[i] < eps ? eps : (p[i] > (1.0f - eps) ? (1.0f - eps) : p[i]);
            float p_t = t[i] * p_clamped + (1.0f - t[i]) * (1.0f - p_clamped);
            float focal_weight = powf(1.0f - p_t, gamma);
            float log_p_t = logf(p_t > eps ? p_t : eps);

            /*
             * d/dp focal = alpha * [ -gamma * (1-p_t)^(gamma-1) * dp_t/dp * log(p_t)
             *                         - (1-p_t)^gamma * (1/p_t) * dp_t/dp ]
             * where dp_t/dp = (2*t - 1)  (since p_t = t*p + (1-t)*(1-p))
             */
            float dp_t_dp = 2.0f * t[i] - 1.0f;
            float term1 = -gamma * powf(1.0f - p_t, gamma - 1.0f) * dp_t_dp * log_p_t;
            float term2 = -focal_weight * (dp_t_dp / (p_t > eps ? p_t : eps));
            g[i] = inv_n * alpha * (term1 + term2);
        }
    }

    return true;
}

/* 6. Huber Loss: 0.5*x^2 if |x|<=delta, else delta*(|x|-0.5*delta) */
bool nimcp_gpu_loss_huber(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* pred,
    const nimcp_gpu_tensor_t* target,
    float* loss,
    nimcp_gpu_tensor_t* grad,
    float delta)
{
    (void)ctx;

    if (!pred || !target || !loss) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_loss_huber: required parameter is NULL");
        return false;
    }
    if (pred->numel != target->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_loss_huber: pred and target size mismatch");
        return false;
    }

    const float* p = tensor_f32(pred);
    const float* t = tensor_f32(target);
    size_t n = pred->numel;
    double sum = 0.0;

    for (size_t i = 0; i < n; i++) {
        float diff = p[i] - t[i];
        float abs_diff = fabsf(diff);
        if (abs_diff <= delta) {
            sum += 0.5 * (double)(diff * diff);
        } else {
            sum += (double)(delta * (abs_diff - 0.5f * delta));
        }
    }
    *loss = (float)(sum / (double)n);

    if (grad) {
        if (grad->numel != n) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                "nimcp_gpu_loss_huber: grad size mismatch");
            return false;
        }
        float* g = tensor_f32(grad);
        float inv_n = 1.0f / (float)n;
        for (size_t i = 0; i < n; i++) {
            float diff = p[i] - t[i];
            float abs_diff = fabsf(diff);
            if (abs_diff <= delta) {
                g[i] = inv_n * diff;
            } else {
                g[i] = inv_n * delta * (diff > 0.0f ? 1.0f : -1.0f);
            }
        }
    }

    return true;
}

/*=============================================================================
 * Gradient Operations (5)
 *=============================================================================*/

/* 7. Gradient accumulate: accum += grad */
bool nimcp_gpu_gradient_accumulate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* grad,
    nimcp_gpu_tensor_t* accum)
{
    (void)ctx;

    if (!grad || !accum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_gradient_accumulate: required parameter is NULL");
        return false;
    }
    if (grad->numel != accum->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_gradient_accumulate: tensor size mismatch");
        return false;
    }

    const float* g = tensor_f32(grad);
    float* a = tensor_f32(accum);
    for (size_t i = 0; i < grad->numel; i++) {
        a[i] += g[i];
    }
    return true;
}

/* 8. Gradient scale: grad *= scale */
bool nimcp_gpu_gradient_scale(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grad,
    float scale)
{
    (void)ctx;

    if (!grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_gradient_scale: grad is NULL");
        return false;
    }

    float* g = tensor_f32(grad);
    for (size_t i = 0; i < grad->numel; i++) {
        g[i] *= scale;
    }
    return true;
}

/* 9. Gradient clip by global norm */
bool nimcp_gpu_gradient_clip_norm(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t** grads,
    size_t n_grads,
    float max_norm,
    float* total_norm)
{
    (void)ctx;

    if (!grads || n_grads == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_gradient_clip_norm: grads is NULL or n_grads is 0");
        return false;
    }

    /* Compute global L2 norm across all gradient tensors */
    double norm_sq = 0.0;
    for (size_t g = 0; g < n_grads; g++) {
        if (!grads[g]) continue;
        const float* data = tensor_f32(grads[g]);
        for (size_t i = 0; i < grads[g]->numel; i++) {
            norm_sq += (double)data[i] * (double)data[i];
        }
    }
    double norm = sqrt(norm_sq);

    if (total_norm) {
        *total_norm = (float)norm;
    }

    /* Scale gradients if norm exceeds max_norm */
    if (norm > (double)max_norm && norm > 0.0) {
        float clip_coef = max_norm / (float)norm;
        for (size_t g = 0; g < n_grads; g++) {
            if (!grads[g]) continue;
            float* data = tensor_f32(grads[g]);
            for (size_t i = 0; i < grads[g]->numel; i++) {
                data[i] *= clip_coef;
            }
        }
    }

    return true;
}

/* 10. Gradient clip by value: clamp to [-clip_value, clip_value] */
bool nimcp_gpu_gradient_clip_value(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grad,
    float clip_value)
{
    (void)ctx;

    if (!grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_gradient_clip_value: grad is NULL");
        return false;
    }

    float* g = tensor_f32(grad);
    for (size_t i = 0; i < grad->numel; i++) {
        if (g[i] > clip_value) {
            g[i] = clip_value;
        } else if (g[i] < -clip_value) {
            g[i] = -clip_value;
        }
    }
    return true;
}

/* 11. Gradient zero: memset to 0 */
bool nimcp_gpu_gradient_zero(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* grad)
{
    (void)ctx;

    if (!grad) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_gradient_zero: grad is NULL");
        return false;
    }

    memset(grad->data, 0, grad->numel * grad->elem_size);
    return true;
}

/*=============================================================================
 * Optimizer State (2)
 *=============================================================================*/

/* 12. Create optimizer state */
nimcp_optim_state_t* nimcp_optim_state_create(
    nimcp_gpu_context_t* ctx,
    nimcp_optim_type_t type,
    const nimcp_gpu_tensor_t* param,
    float lr)
{
    if (!param) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_optim_state_create: param is NULL");
        return NULL;
    }

    nimcp_optim_state_t* state = nimcp_calloc(1, sizeof(nimcp_optim_state_t));
    if (!state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_optim_state_create: allocation failed");
        return NULL;
    }

    state->type = type;
    state->t = 0;
    state->lr = lr;
    state->beta1 = 0.9f;
    state->beta2 = 0.999f;
    state->eps = 1e-8f;
    state->weight_decay = 0.0f;
    state->momentum = 0.0f;
    state->nesterov = false;

    /* Allocate first moment (m) - used by SGD momentum, Adam, etc. */
    state->m = nimcp_gpu_tensor_create(ctx, param->dims, param->ndim, param->precision);
    if (!state->m) {
        nimcp_optim_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_optim_state_create: m tensor allocation failed");
        return NULL;
    }
    memset(state->m->data, 0, state->m->numel * state->m->elem_size);

    /* Allocate second moment (v) - used by Adam, RMSprop, Adagrad, etc. */
    state->v = nimcp_gpu_tensor_create(ctx, param->dims, param->ndim, param->precision);
    if (!state->v) {
        nimcp_optim_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_optim_state_create: v tensor allocation failed");
        return NULL;
    }
    memset(state->v->data, 0, state->v->numel * state->v->elem_size);

    /* Allocate v_hat for AdaDelta */
    state->v_hat = nimcp_gpu_tensor_create(ctx, param->dims, param->ndim, param->precision);
    if (!state->v_hat) {
        nimcp_optim_state_destroy(state);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_optim_state_create: v_hat tensor allocation failed");
        return NULL;
    }
    memset(state->v_hat->data, 0, state->v_hat->numel * state->v_hat->elem_size);

    return state;
}

/* 13. Destroy optimizer state */
void nimcp_optim_state_destroy(nimcp_optim_state_t* state) {
    if (!state) return;

    if (state->m) nimcp_gpu_tensor_destroy(state->m);
    if (state->v) nimcp_gpu_tensor_destroy(state->v);
    if (state->v_hat) nimcp_gpu_tensor_destroy(state->v_hat);
    nimcp_free(state);
}

/*=============================================================================
 * Optimizer Steps (5)
 *=============================================================================*/

/* 14. SGD: p -= lr * (grad + weight_decay*p), with optional momentum */
bool nimcp_gpu_optim_sgd(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    (void)ctx;

    if (!param || !grad || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_optim_sgd: required parameter is NULL");
        return false;
    }
    if (param->numel != grad->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_optim_sgd: param and grad size mismatch");
        return false;
    }

    float* p = tensor_f32(param);
    const float* g = tensor_f32(grad);
    float lr = state->lr;
    float wd = state->weight_decay;
    float mu = state->momentum;
    size_t n = param->numel;

    state->t++;

    if (mu != 0.0f && state->m) {
        /* SGD with momentum */
        float* buf = tensor_f32(state->m);  /* momentum buffer */
        for (size_t i = 0; i < n; i++) {
            float d_p = g[i];
            if (wd != 0.0f) {
                d_p += wd * p[i];
            }
            buf[i] = mu * buf[i] + d_p;

            if (state->nesterov) {
                p[i] -= lr * (d_p + mu * buf[i]);
            } else {
                p[i] -= lr * buf[i];
            }
        }
    } else {
        /* Vanilla SGD */
        for (size_t i = 0; i < n; i++) {
            float d_p = g[i];
            if (wd != 0.0f) {
                d_p += wd * p[i];
            }
            p[i] -= lr * d_p;
        }
    }

    return true;
}

/* 15. Adam optimizer */
bool nimcp_gpu_optim_adam(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    (void)ctx;

    if (!param || !grad || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_optim_adam: required parameter is NULL");
        return false;
    }
    if (!state->m || !state->v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_optim_adam: optimizer state m/v not initialized");
        return false;
    }
    if (param->numel != grad->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_optim_adam: param and grad size mismatch");
        return false;
    }

    float* p = tensor_f32(param);
    const float* g = tensor_f32(grad);
    float* m = tensor_f32(state->m);
    float* v = tensor_f32(state->v);
    size_t n = param->numel;

    state->t++;
    float beta1 = state->beta1;
    float beta2 = state->beta2;
    float lr = state->lr;
    float eps_val = state->eps;
    float wd = state->weight_decay;

    /* Bias correction factors */
    float bc1 = 1.0f - powf(beta1, (float)state->t);
    float bc2 = 1.0f - powf(beta2, (float)state->t);

    for (size_t i = 0; i < n; i++) {
        float gi = g[i];
        /* L2 regularization (combined with gradient for standard Adam) */
        if (wd != 0.0f) {
            gi += wd * p[i];
        }

        /* Update biased first moment */
        m[i] = beta1 * m[i] + (1.0f - beta1) * gi;
        /* Update biased second moment */
        v[i] = beta2 * v[i] + (1.0f - beta2) * gi * gi;

        /* Bias-corrected estimates */
        float m_hat = m[i] / bc1;
        float v_hat = v[i] / bc2;

        /* Update parameter */
        p[i] -= lr * m_hat / (sqrtf(v_hat) + eps_val);
    }

    return true;
}

/* 16. AdamW (decoupled weight decay) */
bool nimcp_gpu_optim_adamw(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    (void)ctx;

    if (!param || !grad || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_optim_adamw: required parameter is NULL");
        return false;
    }
    if (!state->m || !state->v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_optim_adamw: optimizer state m/v not initialized");
        return false;
    }
    if (param->numel != grad->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_optim_adamw: param and grad size mismatch");
        return false;
    }

    float* p = tensor_f32(param);
    const float* g = tensor_f32(grad);
    float* m_buf = tensor_f32(state->m);
    float* v_buf = tensor_f32(state->v);
    size_t n = param->numel;

    state->t++;
    float beta1 = state->beta1;
    float beta2 = state->beta2;
    float lr = state->lr;
    float eps_val = state->eps;
    float wd = state->weight_decay;

    /* Bias correction factors */
    float bc1 = 1.0f - powf(beta1, (float)state->t);
    float bc2 = 1.0f - powf(beta2, (float)state->t);

    for (size_t i = 0; i < n; i++) {
        /* Decoupled weight decay: apply BEFORE Adam update */
        if (wd != 0.0f) {
            p[i] -= lr * wd * p[i];
        }

        float gi = g[i];

        /* Update biased first moment */
        m_buf[i] = beta1 * m_buf[i] + (1.0f - beta1) * gi;
        /* Update biased second moment */
        v_buf[i] = beta2 * v_buf[i] + (1.0f - beta2) * gi * gi;

        /* Bias-corrected estimates */
        float m_hat = m_buf[i] / bc1;
        float v_hat = v_buf[i] / bc2;

        /* Update parameter */
        p[i] -= lr * m_hat / (sqrtf(v_hat) + eps_val);
    }

    return true;
}

/* 17. RMSprop: v = alpha*v + (1-alpha)*g^2, p -= lr*g/sqrt(v+eps) */
bool nimcp_gpu_optim_rmsprop(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    (void)ctx;

    if (!param || !grad || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_optim_rmsprop: required parameter is NULL");
        return false;
    }
    if (!state->v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_optim_rmsprop: optimizer state v not initialized");
        return false;
    }
    if (param->numel != grad->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_optim_rmsprop: param and grad size mismatch");
        return false;
    }

    float* p = tensor_f32(param);
    const float* g = tensor_f32(grad);
    float* v_buf = tensor_f32(state->v);
    size_t n = param->numel;

    state->t++;
    /* RMSprop uses beta2 as alpha (decay rate for running average) */
    float alpha_decay = state->beta2;
    float lr = state->lr;
    float eps_val = state->eps;
    float wd = state->weight_decay;

    for (size_t i = 0; i < n; i++) {
        float gi = g[i];
        if (wd != 0.0f) {
            gi += wd * p[i];
        }

        /* Update running average of squared gradient */
        v_buf[i] = alpha_decay * v_buf[i] + (1.0f - alpha_decay) * gi * gi;

        /* Update parameter */
        p[i] -= lr * gi / (sqrtf(v_buf[i]) + eps_val);
    }

    return true;
}

/* 18. Adagrad: v += g^2, p -= lr*g/sqrt(v+eps) */
bool nimcp_gpu_optim_adagrad(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* param,
    const nimcp_gpu_tensor_t* grad,
    nimcp_optim_state_t* state)
{
    (void)ctx;

    if (!param || !grad || !state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_optim_adagrad: required parameter is NULL");
        return false;
    }
    if (!state->v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_optim_adagrad: optimizer state v not initialized");
        return false;
    }
    if (param->numel != grad->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_optim_adagrad: param and grad size mismatch");
        return false;
    }

    float* p = tensor_f32(param);
    const float* g = tensor_f32(grad);
    float* v_buf = tensor_f32(state->v);
    size_t n = param->numel;

    state->t++;
    float lr = state->lr;
    float eps_val = state->eps;
    float wd = state->weight_decay;

    for (size_t i = 0; i < n; i++) {
        float gi = g[i];
        if (wd != 0.0f) {
            gi += wd * p[i];
        }

        /* Accumulate squared gradient (monotonically increasing) */
        v_buf[i] += gi * gi;

        /* Update parameter */
        p[i] -= lr * gi / (sqrtf(v_buf[i]) + eps_val);
    }

    return true;
}

/*=============================================================================
 * Backpropagation Kernels (9)
 *=============================================================================*/

/* 19. Backward linear: y = x @ W^T + b */
bool nimcp_gpu_backward_linear(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* weight,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_weight,
    nimcp_gpu_tensor_t* grad_bias)
{
    (void)ctx;

    if (!x || !weight || !grad_output || !grad_input || !grad_weight) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_backward_linear: required parameter is NULL");
        return false;
    }

    /*
     * x:           (batch, in_features)
     * weight:      (out_features, in_features)
     * grad_output: (batch, out_features)
     * grad_input:  (batch, in_features)
     * grad_weight: (out_features, in_features)
     * grad_bias:   (out_features,) - optional
     */
    size_t batch = 1;
    size_t in_features, out_features;

    if (x->ndim >= 2) {
        batch = x->dims[0];
        in_features = x->dims[1];
    } else {
        in_features = x->dims[0];
    }

    if (weight->ndim >= 2) {
        out_features = weight->dims[0];
    } else {
        out_features = weight->dims[0];
    }

    const float* x_data = tensor_f32(x);
    const float* w_data = tensor_f32(weight);
    const float* dy = tensor_f32(grad_output);
    float* dx = tensor_f32(grad_input);
    float* dw = tensor_f32(grad_weight);

    /* grad_input = grad_output @ weight */
    /* dx[b][i] = sum_o(dy[b][o] * W[o][i]) */
    memset(dx, 0, grad_input->numel * sizeof(float));
    for (size_t b = 0; b < batch; b++) {
        for (size_t o = 0; o < out_features; o++) {
            float dy_val = dy[b * out_features + o];
            for (size_t i = 0; i < in_features; i++) {
                dx[b * in_features + i] += dy_val * w_data[o * in_features + i];
            }
        }
    }

    /* grad_weight = x^T @ grad_output  =>  dW[o][i] = sum_b(dy[b][o] * x[b][i]) */
    memset(dw, 0, grad_weight->numel * sizeof(float));
    for (size_t b = 0; b < batch; b++) {
        for (size_t o = 0; o < out_features; o++) {
            float dy_val = dy[b * out_features + o];
            for (size_t i = 0; i < in_features; i++) {
                dw[o * in_features + i] += dy_val * x_data[b * in_features + i];
            }
        }
    }

    /* grad_bias = sum(grad_output, axis=0)  =>  db[o] = sum_b(dy[b][o]) */
    if (grad_bias) {
        float* db = tensor_f32(grad_bias);
        memset(db, 0, grad_bias->numel * sizeof(float));
        for (size_t b = 0; b < batch; b++) {
            for (size_t o = 0; o < out_features; o++) {
                db[o] += dy[b * out_features + o];
            }
        }
    }

    return true;
}

/* 20. Backward ReLU: dx = dy * (x > 0) */
bool nimcp_gpu_backward_relu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    (void)ctx;

    if (!x || !grad_output || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_backward_relu: required parameter is NULL");
        return false;
    }
    if (x->numel != grad_output->numel || x->numel != grad_input->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_backward_relu: tensor size mismatch");
        return false;
    }

    const float* x_data = tensor_f32(x);
    const float* dy = tensor_f32(grad_output);
    float* dx = tensor_f32(grad_input);
    size_t n = x->numel;

    for (size_t i = 0; i < n; i++) {
        dx[i] = x_data[i] > 0.0f ? dy[i] : 0.0f;
    }
    return true;
}

/* 21. Backward sigmoid: dx = dy * output * (1 - output) */
bool nimcp_gpu_backward_sigmoid(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    (void)ctx;

    if (!output || !grad_output || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_backward_sigmoid: required parameter is NULL");
        return false;
    }
    if (output->numel != grad_output->numel || output->numel != grad_input->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_backward_sigmoid: tensor size mismatch");
        return false;
    }

    const float* s = tensor_f32(output);
    const float* dy = tensor_f32(grad_output);
    float* dx = tensor_f32(grad_input);
    size_t n = output->numel;

    for (size_t i = 0; i < n; i++) {
        dx[i] = dy[i] * s[i] * (1.0f - s[i]);
    }
    return true;
}

/* 22. Backward tanh: dx = dy * (1 - output^2) */
bool nimcp_gpu_backward_tanh(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    (void)ctx;

    if (!output || !grad_output || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_backward_tanh: required parameter is NULL");
        return false;
    }
    if (output->numel != grad_output->numel || output->numel != grad_input->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_backward_tanh: tensor size mismatch");
        return false;
    }

    const float* t_out = tensor_f32(output);
    const float* dy = tensor_f32(grad_output);
    float* dx = tensor_f32(grad_input);
    size_t n = output->numel;

    for (size_t i = 0; i < n; i++) {
        dx[i] = dy[i] * (1.0f - t_out[i] * t_out[i]);
    }
    return true;
}

/* 23. Backward GELU (approximate derivative) */
bool nimcp_gpu_backward_gelu(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    (void)ctx;

    if (!x || !grad_output || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_backward_gelu: required parameter is NULL");
        return false;
    }
    if (x->numel != grad_output->numel || x->numel != grad_input->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_backward_gelu: tensor size mismatch");
        return false;
    }

    const float* x_data = tensor_f32(x);
    const float* dy = tensor_f32(grad_output);
    float* dx = tensor_f32(grad_input);
    size_t n = x->numel;

    /*
     * GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
     *
     * Derivative using approximate form:
     * Let k = sqrt(2/pi), a = 0.044715
     * Let u = k * (x + a * x^3)
     * Let t = tanh(u)
     * GELU'(x) = 0.5 * (1 + t) + 0.5 * x * (1 - t^2) * k * (1 + 3*a*x^2)
     */
    const float k = 0.7978845608f;  /* sqrt(2/pi) */
    const float a = 0.044715f;

    for (size_t i = 0; i < n; i++) {
        float xi = x_data[i];
        float x3 = xi * xi * xi;
        float u = k * (xi + a * x3);
        float t = tanhf(u);
        float sech2 = 1.0f - t * t;
        float du_dx = k * (1.0f + 3.0f * a * xi * xi);
        float gelu_prime = 0.5f * (1.0f + t) + 0.5f * xi * sech2 * du_dx;
        dx[i] = dy[i] * gelu_prime;
    }
    return true;
}

/* 24. Backward softmax: dx_i = s_i * (dy_i - sum_j(dy_j * s_j)) */
bool nimcp_gpu_backward_softmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* output,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input)
{
    (void)ctx;

    if (!output || !grad_output || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_backward_softmax: required parameter is NULL");
        return false;
    }
    if (output->numel != grad_output->numel || output->numel != grad_input->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_backward_softmax: tensor size mismatch");
        return false;
    }

    const float* s = tensor_f32(output);
    const float* dy = tensor_f32(grad_output);
    float* dx = tensor_f32(grad_input);

    /*
     * Apply along last dimension. Treat as (batch_size, num_classes).
     * If 1D, batch_size = 1, num_classes = numel.
     */
    size_t batch_size = 1;
    size_t num_classes = output->numel;
    if (output->ndim >= 2) {
        batch_size = output->dims[0];
        num_classes = output->dims[output->ndim - 1];
    }

    for (size_t b = 0; b < batch_size; b++) {
        const float* s_row = s + b * num_classes;
        const float* dy_row = dy + b * num_classes;
        float* dx_row = dx + b * num_classes;

        /* Compute sum_j(dy_j * s_j) */
        double dot = 0.0;
        for (size_t j = 0; j < num_classes; j++) {
            dot += (double)dy_row[j] * (double)s_row[j];
        }

        /* dx_i = s_i * (dy_i - dot) */
        for (size_t j = 0; j < num_classes; j++) {
            dx_row[j] = s_row[j] * (dy_row[j] - (float)dot);
        }
    }

    return true;
}

/* 25. Backward batch normalization */
bool nimcp_gpu_backward_batchnorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* mean,
    const nimcp_gpu_tensor_t* var,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_gamma,
    nimcp_gpu_tensor_t* grad_beta,
    float eps)
{
    (void)ctx;

    if (!x || !gamma || !mean || !var || !grad_output || !grad_input ||
        !grad_gamma || !grad_beta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_backward_batchnorm: required parameter is NULL");
        return false;
    }

    /*
     * x:           (N, C) or (N, C, H, W) - normalize along batch dimension (0)
     * gamma, mean, var: (C,)
     * grad_output:  same shape as x
     *
     * For simplicity, treat as (N, C) where:
     *   N = total samples = x->dims[0]
     *   C = channels = gamma->numel
     *   spatial = product of remaining dims
     */
    size_t N = x->dims[0];
    size_t C = gamma->numel;
    size_t spatial = 1;
    for (uint32_t d = 2; d < x->ndim; d++) {
        spatial *= x->dims[d];
    }
    size_t M = N * spatial;  /* number of elements per channel */

    const float* x_data = tensor_f32(x);
    const float* gamma_data = tensor_f32(gamma);
    const float* mean_data = tensor_f32(mean);
    const float* var_data = tensor_f32(var);
    const float* dy = tensor_f32(grad_output);
    float* dx = tensor_f32(grad_input);
    float* dgamma = tensor_f32(grad_gamma);
    float* dbeta = tensor_f32(grad_beta);

    memset(dgamma, 0, C * sizeof(float));
    memset(dbeta, 0, C * sizeof(float));

    /* Pass 1: compute dgamma, dbeta, and intermediate sums for dx */
    /* Also compute sum_dy and sum_dy_xhat per channel */
    float* sum_dy = nimcp_calloc(C, sizeof(float));
    float* sum_dy_xhat = nimcp_calloc(C, sizeof(float));
    if (!sum_dy || !sum_dy_xhat) {
        nimcp_free(sum_dy);
        nimcp_free(sum_dy_xhat);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_gpu_backward_batchnorm: temp allocation failed");
        return false;
    }

    for (size_t n = 0; n < N; n++) {
        for (size_t c = 0; c < C; c++) {
            float inv_std = 1.0f / sqrtf(var_data[c] + eps);
            for (size_t s = 0; s < spatial; s++) {
                size_t idx = n * C * spatial + c * spatial + s;
                float x_hat = (x_data[idx] - mean_data[c]) * inv_std;
                float dy_val = dy[idx];

                dgamma[c] += dy_val * x_hat;
                dbeta[c] += dy_val;
                sum_dy[c] += dy_val;
                sum_dy_xhat[c] += dy_val * x_hat;
            }
        }
    }

    /* Pass 2: compute dx */
    float inv_M = 1.0f / (float)M;
    for (size_t n = 0; n < N; n++) {
        for (size_t c = 0; c < C; c++) {
            float inv_std = 1.0f / sqrtf(var_data[c] + eps);
            for (size_t s = 0; s < spatial; s++) {
                size_t idx = n * C * spatial + c * spatial + s;
                float x_hat = (x_data[idx] - mean_data[c]) * inv_std;
                /*
                 * dx = gamma / (M * std) * (M * dy - sum_dy - x_hat * sum_dy_xhat)
                 */
                dx[idx] = gamma_data[c] * inv_std * inv_M *
                          ((float)M * dy[idx] - sum_dy[c] - x_hat * sum_dy_xhat[c]);
            }
        }
    }

    nimcp_free(sum_dy);
    nimcp_free(sum_dy_xhat);
    return true;
}

/* 26. Backward layer normalization */
bool nimcp_gpu_backward_layernorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* mean,
    const nimcp_gpu_tensor_t* var,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    nimcp_gpu_tensor_t* grad_gamma,
    nimcp_gpu_tensor_t* grad_beta,
    float eps)
{
    (void)ctx;

    if (!x || !gamma || !mean || !var || !grad_output || !grad_input ||
        !grad_gamma || !grad_beta) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_backward_layernorm: required parameter is NULL");
        return false;
    }

    /*
     * Layer norm normalizes along the LAST dimension(s).
     * x:           (N, D) where N = batch, D = normalized dimension
     * gamma, beta: (D,)
     * mean, var:   (N,) - one per sample
     *
     * For simplicity: treat as (N, D)
     */
    size_t D = gamma->numel;
    size_t N = x->numel / D;

    const float* x_data = tensor_f32(x);
    const float* gamma_data = tensor_f32(gamma);
    const float* mean_data = tensor_f32(mean);
    const float* var_data = tensor_f32(var);
    const float* dy = tensor_f32(grad_output);
    float* dx = tensor_f32(grad_input);
    float* dgamma = tensor_f32(grad_gamma);
    float* dbeta = tensor_f32(grad_beta);

    memset(dgamma, 0, D * sizeof(float));
    memset(dbeta, 0, D * sizeof(float));

    /* Pass 1: accumulate dgamma and dbeta */
    for (size_t n = 0; n < N; n++) {
        float inv_std = 1.0f / sqrtf(var_data[n] + eps);
        for (size_t d = 0; d < D; d++) {
            size_t idx = n * D + d;
            float x_hat = (x_data[idx] - mean_data[n]) * inv_std;
            dgamma[d] += dy[idx] * x_hat;
            dbeta[d] += dy[idx];
        }
    }

    /* Pass 2: compute dx for each sample */
    float inv_D = 1.0f / (float)D;
    for (size_t n = 0; n < N; n++) {
        float inv_std = 1.0f / sqrtf(var_data[n] + eps);
        const float* dy_row = dy + n * D;
        const float* x_row = x_data + n * D;
        float* dx_row = dx + n * D;

        /* Compute per-sample sums */
        double sum_dy_gamma = 0.0;
        double sum_dy_gamma_xhat = 0.0;
        for (size_t d = 0; d < D; d++) {
            float dy_g = dy_row[d] * gamma_data[d];
            float x_hat = (x_row[d] - mean_data[n]) * inv_std;
            sum_dy_gamma += (double)dy_g;
            sum_dy_gamma_xhat += (double)(dy_g * x_hat);
        }

        /* dx = inv_std / D * (D * dy*gamma - sum_dy_gamma - x_hat * sum_dy_gamma_xhat) */
        for (size_t d = 0; d < D; d++) {
            float x_hat = (x_row[d] - mean_data[n]) * inv_std;
            float dy_g = dy_row[d] * gamma_data[d];
            dx_row[d] = inv_std * inv_D *
                        ((float)D * dy_g - (float)sum_dy_gamma - x_hat * (float)sum_dy_gamma_xhat);
        }
    }

    return true;
}

/* 27. Backward dropout: dx = dy * mask / (1-p) */
bool nimcp_gpu_backward_dropout(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* mask,
    const nimcp_gpu_tensor_t* grad_output,
    nimcp_gpu_tensor_t* grad_input,
    float p)
{
    (void)ctx;

    if (!mask || !grad_output || !grad_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_gpu_backward_dropout: required parameter is NULL");
        return false;
    }
    if (mask->numel != grad_output->numel || mask->numel != grad_input->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_gpu_backward_dropout: tensor size mismatch");
        return false;
    }

    /* Avoid division by zero if p == 1 */
    if (p >= 1.0f) {
        memset(grad_input->data, 0, grad_input->numel * sizeof(float));
        return true;
    }

    const float* m = tensor_f32(mask);
    const float* dy = tensor_f32(grad_output);
    float* dx = tensor_f32(grad_input);
    size_t n = mask->numel;
    float inv_keep = 1.0f / (1.0f - p);

    for (size_t i = 0; i < n; i++) {
        dx[i] = dy[i] * m[i] * inv_keep;
    }
    return true;
}

/*=============================================================================
 * Learning Rate Schedulers (4)
 *=============================================================================*/

/* 28. Step LR: lr = initial_lr * gamma^(step / step_size) */
float nimcp_lr_step(
    float initial_lr,
    uint64_t step,
    uint64_t step_size,
    float gamma)
{
    if (step_size == 0) return initial_lr;
    uint64_t num_decays = step / step_size;
    return initial_lr * powf(gamma, (float)num_decays);
}

/* 29. Cosine annealing LR */
float nimcp_lr_cosine(
    float max_lr,
    float min_lr,
    uint64_t step,
    uint64_t total_steps)
{
    if (total_steps == 0) return max_lr;
    if (step >= total_steps) return min_lr;

    double progress = (double)step / (double)total_steps;
    double cosine_factor = 0.5 * (1.0 + cos(M_PI * progress));
    return min_lr + (float)((double)(max_lr - min_lr) * cosine_factor);
}

/* 30. Linear warmup then linear decay */
float nimcp_lr_warmup_linear(
    float max_lr,
    uint64_t step,
    uint64_t warmup_steps,
    uint64_t total_steps)
{
    if (total_steps == 0) return max_lr;

    if (step < warmup_steps) {
        /* Linear warmup: 0 -> max_lr */
        if (warmup_steps == 0) return max_lr;
        return max_lr * (float)step / (float)warmup_steps;
    } else {
        /* Linear decay: max_lr -> 0 */
        uint64_t decay_steps = total_steps - warmup_steps;
        if (decay_steps == 0) return max_lr;
        uint64_t elapsed = step - warmup_steps;
        if (elapsed >= decay_steps) return 0.0f;
        return max_lr * (1.0f - (float)elapsed / (float)decay_steps);
    }
}

/* 31. Exponential decay LR: lr = initial_lr * exp(-decay_rate * step) */
float nimcp_lr_exponential(
    float initial_lr,
    uint64_t step,
    float decay_rate)
{
    return initial_lr * expf(-decay_rate * (float)step);
}

/* 32. Cross-network bridge GPU GEMV stub — always returns false (CPU fallback) */
bool nimcp_gpu_bridge_gemv(
    void* gpu_ctx,
    const float* W,
    const float* x,
    const float* bias,
    float* y,
    uint32_t rows,
    uint32_t cols,
    void** p_d_W,
    void** p_d_x,
    void** p_d_y,
    uint32_t* p_buf_rows,
    uint32_t* p_buf_cols)
{
    (void)gpu_ctx; (void)W; (void)x; (void)bias; (void)y; (void)rows; (void)cols;
    (void)p_d_W; (void)p_d_x; (void)p_d_y; (void)p_buf_rows; (void)p_buf_cols;
    return false; /* No GPU — caller falls back to CPU matmul */
}
