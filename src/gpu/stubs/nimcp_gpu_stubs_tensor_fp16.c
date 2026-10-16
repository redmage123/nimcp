/**
 * @file nimcp_gpu_stubs_tensor_fp16.c
 * @brief CPU fallback implementations for FP16/mixed precision tensor operations and AMP autocast
 *
 * WHAT: CPU implementations for FP16 ops, loss scaling, AMP context, and autocast
 * WHY:  Enables mixed-precision workflows on CPU-only systems (all ops use FP32 internally)
 * HOW:  FP16 ops delegate to FP32 math; loss scaler tracks scale/backoff; autocast is pass-through
 */

#include "gpu/tensor/nimcp_tensor_fp16.h"
#include "gpu/tensor/nimcp_amp_autocast.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>

/*=============================================================================
 * Mixed Precision Tensor - CPU fallback (all in FP32)
 *=============================================================================*/

nimcp_mp_tensor_t* nimcp_mp_tensor_create(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* fp32_tensor,
    nimcp_mp_dtype_t compute_dtype,
    bool keep_master)
{
    (void)ctx;
    if (!fp32_tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mp_tensor_create: fp32_tensor is NULL");
        return NULL;
    }

    nimcp_mp_tensor_t* mp = nimcp_calloc(1, sizeof(nimcp_mp_tensor_t));
    if (!mp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mp_tensor_create: allocation failed");
        return NULL;
    }

    mp->compute_dtype = compute_dtype;
    mp->has_master = keep_master;

    /* On CPU, both compute and master are FP32 clones */
    mp->fp16_data = nimcp_gpu_tensor_clone(fp32_tensor);
    mp->owns_compute = true;
    if (!mp->fp16_data) {
        nimcp_free(mp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mp_tensor_create: compute clone failed");
        return NULL;
    }

    if (keep_master) {
        mp->fp32_master = nimcp_gpu_tensor_clone(fp32_tensor);
        mp->owns_master = true;
        if (!mp->fp32_master) {
            nimcp_gpu_tensor_destroy(mp->fp16_data);
            nimcp_free(mp);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mp_tensor_create: master clone failed");
            return NULL;
        }
    }

    return mp;
}

nimcp_mp_tensor_t* nimcp_mp_tensor_create_empty(
    nimcp_gpu_context_t* ctx,
    const size_t* dims,
    uint32_t ndim,
    nimcp_mp_dtype_t compute_dtype,
    bool keep_master)
{
    if (!dims) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mp_tensor_create_empty: dims is NULL");
        return NULL;
    }

    nimcp_mp_tensor_t* mp = nimcp_calloc(1, sizeof(nimcp_mp_tensor_t));
    if (!mp) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mp_tensor_create_empty: allocation failed");
        return NULL;
    }

    mp->compute_dtype = compute_dtype;
    mp->has_master = keep_master;

    /* On CPU, everything is FP32 */
    mp->fp16_data = nimcp_gpu_tensor_create(ctx, dims, ndim, NIMCP_GPU_PRECISION_FP32);
    mp->owns_compute = true;
    if (!mp->fp16_data) {
        nimcp_free(mp);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mp_tensor_create_empty: compute alloc failed");
        return NULL;
    }

    if (keep_master) {
        mp->fp32_master = nimcp_gpu_tensor_create(ctx, dims, ndim, NIMCP_GPU_PRECISION_FP32);
        mp->owns_master = true;
        if (!mp->fp32_master) {
            nimcp_gpu_tensor_destroy(mp->fp16_data);
            nimcp_free(mp);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_mp_tensor_create_empty: master alloc failed");
            return NULL;
        }
    }

    return mp;
}

void nimcp_mp_tensor_destroy(nimcp_mp_tensor_t* tensor) {
    if (!tensor) return;
    if (tensor->owns_compute && tensor->fp16_data) nimcp_gpu_tensor_destroy(tensor->fp16_data);
    if (tensor->owns_master && tensor->fp32_master) nimcp_gpu_tensor_destroy(tensor->fp32_master);
    nimcp_free(tensor);
}

bool nimcp_mp_tensor_sync_compute(nimcp_gpu_context_t* ctx, nimcp_mp_tensor_t* tensor) {
    (void)ctx;
    if (!tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mp_tensor_sync_compute: tensor is NULL");
        return false;
    }
    /* On CPU both are FP32, copy master -> compute */
    if (tensor->fp32_master && tensor->fp16_data &&
        tensor->fp32_master->data && tensor->fp16_data->data) {
        size_t bytes = tensor->fp16_data->numel * tensor->fp16_data->elem_size;
        memcpy(tensor->fp16_data->data, tensor->fp32_master->data, bytes);
    }
    return true;
}

bool nimcp_mp_tensor_sync_master(nimcp_gpu_context_t* ctx, nimcp_mp_tensor_t* tensor) {
    (void)ctx;
    if (!tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mp_tensor_sync_master: tensor is NULL");
        return false;
    }
    /* Copy compute -> master */
    if (tensor->fp32_master && tensor->fp16_data &&
        tensor->fp32_master->data && tensor->fp16_data->data) {
        size_t bytes = tensor->fp32_master->numel * tensor->fp32_master->elem_size;
        memcpy(tensor->fp32_master->data, tensor->fp16_data->data, bytes);
    }
    return true;
}

/*=============================================================================
 * Loss Scaler - CPU implementation with dynamic scaling
 *=============================================================================*/

nimcp_loss_scaler_t* nimcp_loss_scaler_create(bool dynamic) {
    nimcp_loss_scaler_t* s = nimcp_calloc(1, sizeof(nimcp_loss_scaler_t));
    if (!s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_loss_scaler_create: allocation failed");
        return NULL;
    }
    s->scale = MP_DEFAULT_INIT_SCALE;
    s->growth_factor = MP_DEFAULT_GROWTH_FACTOR;
    s->backoff_factor = MP_DEFAULT_BACKOFF_FACTOR;
    s->min_scale = MP_MIN_SCALE;
    s->max_scale = (float)MP_MAX_SCALE;
    s->growth_interval = MP_DEFAULT_GROWTH_INTERVAL;
    s->consecutive_ok = 0;
    s->dynamic = dynamic;
    return s;
}

nimcp_loss_scaler_t* nimcp_loss_scaler_create_custom(
    float init_scale, float growth_factor, float backoff_factor,
    int growth_interval, bool dynamic)
{
    nimcp_loss_scaler_t* s = nimcp_calloc(1, sizeof(nimcp_loss_scaler_t));
    if (!s) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_loss_scaler_create_custom: allocation failed");
        return NULL;
    }
    s->scale = init_scale;
    s->growth_factor = growth_factor;
    s->backoff_factor = backoff_factor;
    s->min_scale = MP_MIN_SCALE;
    s->max_scale = (float)MP_MAX_SCALE;
    s->growth_interval = growth_interval;
    s->consecutive_ok = 0;
    s->dynamic = dynamic;
    return s;
}

void nimcp_loss_scaler_destroy(nimcp_loss_scaler_t* scaler) {
    nimcp_free(scaler);
}

float nimcp_loss_scaler_scale(nimcp_loss_scaler_t* scaler, float loss) {
    if (!scaler) return loss;
    return loss * scaler->scale;
}

bool nimcp_loss_scaler_unscale(
    nimcp_gpu_context_t* ctx,
    nimcp_loss_scaler_t* scaler,
    nimcp_gpu_tensor_t* gradients)
{
    (void)ctx;
    if (!scaler || !gradients || !gradients->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_loss_scaler_unscale: required parameter is NULL");
        return false;
    }
    float inv_scale = 1.0f / scaler->scale;
    float* data = (float*)gradients->data;
    for (size_t i = 0; i < gradients->numel; i++) {
        data[i] *= inv_scale;
    }
    return true;
}

bool nimcp_loss_scaler_unscale_fp16(
    nimcp_gpu_context_t* ctx,
    nimcp_loss_scaler_t* scaler,
    nimcp_gpu_tensor_t* gradients)
{
    /* On CPU, FP16 and FP32 unscale are identical */
    return nimcp_loss_scaler_unscale(ctx, scaler, gradients);
}

void nimcp_loss_scaler_update(nimcp_loss_scaler_t* scaler, bool gradients_valid) {
    if (!scaler || !scaler->dynamic) return;

    scaler->total_steps++;

    if (gradients_valid) {
        scaler->consecutive_ok++;
        if (scaler->consecutive_ok >= scaler->growth_interval) {
            float new_scale = scaler->scale * scaler->growth_factor;
            if (new_scale <= scaler->max_scale) {
                scaler->scale = new_scale;
                scaler->scale_increases++;
            }
            scaler->consecutive_ok = 0;
        }
    } else {
        scaler->scale *= scaler->backoff_factor;
        if (scaler->scale < scaler->min_scale) {
            scaler->scale = scaler->min_scale;
        }
        scaler->consecutive_ok = 0;
        scaler->overflow_count++;
        scaler->scale_decreases++;
    }
}

float nimcp_loss_scaler_get_scale(const nimcp_loss_scaler_t* scaler) {
    if (!scaler) return 1.0f;
    return scaler->scale;
}

bool nimcp_loss_scaler_should_skip(const nimcp_loss_scaler_t* scaler, bool gradients_valid) {
    (void)scaler;
    return !gradients_valid;
}

/*=============================================================================
 * AMP Context - CPU fallback (no actual mixed precision)
 *=============================================================================*/

nimcp_amp_context_t* nimcp_amp_create(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_mp_dtype_t compute_dtype,
    bool enable_scaler)
{
    nimcp_amp_context_t* ctx = nimcp_calloc(1, sizeof(nimcp_amp_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_amp_create: allocation failed");
        return NULL;
    }
    ctx->gpu_ctx = gpu_ctx;
    ctx->default_dtype = compute_dtype;
    ctx->enabled = true;
    ctx->autocasting = false;

    /* On CPU, all ops use FP32 */
    ctx->matmul_dtype = MP_DTYPE_FP32;
    ctx->conv_dtype = MP_DTYPE_FP32;
    ctx->norm_dtype = MP_DTYPE_FP32;
    ctx->softmax_dtype = MP_DTYPE_FP32;
    ctx->loss_dtype = MP_DTYPE_FP32;

    ctx->tensor_cores_available = false;
    ctx->bf16_supported = false;

    if (enable_scaler) {
        ctx->scaler = nimcp_loss_scaler_create(true);
    }

    return ctx;
}

void nimcp_amp_destroy(nimcp_amp_context_t* ctx) {
    if (!ctx) return;
    if (ctx->scaler) nimcp_loss_scaler_destroy(ctx->scaler);
    nimcp_free(ctx);
}

bool nimcp_amp_autocast_enter(nimcp_amp_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_amp_autocast_enter: ctx is NULL");
        return false;
    }
    ctx->autocasting = true;
    return true;
}

bool nimcp_amp_autocast_exit(nimcp_amp_context_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_amp_autocast_exit: ctx is NULL");
        return false;
    }
    ctx->autocasting = false;
    return true;
}

bool nimcp_amp_is_autocasting(const nimcp_amp_context_t* ctx) {
    if (!ctx) return false;
    return ctx->autocasting;
}

nimcp_mp_dtype_t nimcp_amp_get_dtype(const nimcp_amp_context_t* ctx, nimcp_mp_op_category_t category) {
    (void)category;
    if (!ctx) return MP_DTYPE_FP32;
    /* On CPU, everything stays FP32 */
    return MP_DTYPE_FP32;
}

nimcp_gpu_tensor_t* nimcp_amp_cast_tensor(
    nimcp_amp_context_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    nimcp_mp_op_category_t category)
{
    (void)category;
    if (ctx) ctx->fp32_ops++;
    /* On CPU, no casting needed - return tensor as-is */
    return tensor;
}

/*=============================================================================
 * FP16 Conversion Kernels - CPU fallback (all FP32, conversions are copies)
 *=============================================================================*/

bool nimcp_fp32_to_fp16(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src, nimcp_gpu_tensor_t* dst) {
    (void)ctx;
    if (!src || !dst || !src->data || !dst->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fp32_to_fp16: required parameter is NULL");
        return false;
    }
    size_t n = src->numel < dst->numel ? src->numel : dst->numel;
    memcpy(dst->data, src->data, n * sizeof(float));
    return true;
}

bool nimcp_fp16_to_fp32(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src, nimcp_gpu_tensor_t* dst) {
    (void)ctx;
    if (!src || !dst || !src->data || !dst->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fp16_to_fp32: required parameter is NULL");
        return false;
    }
    size_t n = src->numel < dst->numel ? src->numel : dst->numel;
    memcpy(dst->data, src->data, n * sizeof(float));
    return true;
}

bool nimcp_fp32_to_bf16(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src, nimcp_gpu_tensor_t* dst) {
    (void)ctx;
    if (!src || !dst || !src->data || !dst->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fp32_to_bf16: required parameter is NULL");
        return false;
    }
    size_t n = src->numel < dst->numel ? src->numel : dst->numel;
    memcpy(dst->data, src->data, n * sizeof(float));
    return true;
}

bool nimcp_bf16_to_fp32(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* src, nimcp_gpu_tensor_t* dst) {
    (void)ctx;
    if (!src || !dst || !src->data || !dst->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_bf16_to_fp32: required parameter is NULL");
        return false;
    }
    size_t n = src->numel < dst->numel ? src->numel : dst->numel;
    memcpy(dst->data, src->data, n * sizeof(float));
    return true;
}

/*=============================================================================
 * FP16 Element-wise Operations (CPU: delegate to FP32 implementations)
 *=============================================================================*/

bool nimcp_fp16_add(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a, const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) {
    return nimcp_gpu_add(ctx, a, b, out);
}

bool nimcp_fp16_mul(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a, const nimcp_gpu_tensor_t* b, nimcp_gpu_tensor_t* out) {
    return nimcp_gpu_mul(ctx, a, b, out);
}

bool nimcp_fp16_scale(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, float scale, nimcp_gpu_tensor_t* out) {
    return nimcp_gpu_mul_scalar(ctx, x, scale, out);
}

bool nimcp_fp16_fma(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* a, const nimcp_gpu_tensor_t* b, const nimcp_gpu_tensor_t* c, nimcp_gpu_tensor_t* out) {
    (void)ctx;
    if (!a || !b || !c || !out || !a->data || !b->data || !c->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fp16_fma: required parameter is NULL");
        return false;
    }
    size_t n = out->numel;
    const float* sa = (const float*)a->data;
    const float* sb = (const float*)b->data;
    const float* sc = (const float*)c->data;
    float* dst = (float*)out->data;
    for (size_t i = 0; i < n; i++) {
        size_t ai = (i < a->numel) ? i : a->numel - 1;
        size_t bi = (i < b->numel) ? i : b->numel - 1;
        size_t ci = (i < c->numel) ? i : c->numel - 1;
        dst[i] = fmaf(sa[ai], sb[bi], sc[ci]);
    }
    return true;
}

/*=============================================================================
 * FP16 GEMM Operations
 *=============================================================================*/

bool nimcp_fp16_gemm(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A, const nimcp_gpu_tensor_t* B, nimcp_gpu_tensor_t* C,
                     float alpha, float beta, bool trans_a, bool trans_b) {
    return nimcp_gpu_gemm(ctx, A, B, C, alpha, beta, trans_a, trans_b);
}

bool nimcp_fp16_gemm_batched(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* A, const nimcp_gpu_tensor_t* B, nimcp_gpu_tensor_t* C,
                             float alpha, float beta, bool trans_a, bool trans_b) {
    return nimcp_gpu_gemm_batched(ctx, A, B, C, alpha, beta, trans_a, trans_b);
}

/*=============================================================================
 * FP16 Activation Functions
 *=============================================================================*/

bool nimcp_fp16_relu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) {
    return nimcp_gpu_relu(ctx, x, out);
}

bool nimcp_fp16_gelu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) {
    return nimcp_gpu_gelu(ctx, x, out);
}

bool nimcp_fp16_sigmoid(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) {
    return nimcp_gpu_sigmoid(ctx, x, out);
}

bool nimcp_fp16_tanh(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) {
    return nimcp_gpu_tanh(ctx, x, out);
}

bool nimcp_fp16_silu(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) {
    return nimcp_gpu_silu(ctx, x, out);
}

/*=============================================================================
 * Numerically Stable Operations
 *=============================================================================*/

bool nimcp_fp16_softmax_stable(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* x, nimcp_gpu_tensor_t* out) {
    return nimcp_gpu_softmax(ctx, x, out);
}

bool nimcp_fp16_layernorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* out,
    float eps)
{
    (void)ctx;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fp16_layernorm: required parameter is NULL");
        return false;
    }

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;
    const float* g = gamma ? (const float*)gamma->data : NULL;
    const float* b = beta ? (const float*)beta->data : NULL;

    /* Last dimension is the normalization dimension */
    size_t norm_size = x->dims[x->ndim - 1];
    size_t batch_size = x->numel / norm_size;

    for (size_t batch = 0; batch < batch_size; batch++) {
        const float* row = src + batch * norm_size;
        float* out_row = dst + batch * norm_size;

        /* Compute mean */
        double mean = 0.0;
        for (size_t i = 0; i < norm_size; i++) mean += (double)row[i];
        mean /= (double)norm_size;

        /* Compute variance */
        double var = 0.0;
        for (size_t i = 0; i < norm_size; i++) {
            double diff = (double)row[i] - mean;
            var += diff * diff;
        }
        var /= (double)norm_size;

        /* Normalize */
        double inv_std = 1.0 / sqrt(var + (double)eps);
        for (size_t i = 0; i < norm_size; i++) {
            float normalized = (float)(((double)row[i] - mean) * inv_std);
            float gi = (g && gamma && i < gamma->numel) ? g[i] : 1.0f;
            float bi = (b && beta && i < beta->numel) ? b[i] : 0.0f;
            out_row[i] = normalized * gi + bi;
        }
    }
    return true;
}

bool nimcp_fp16_batchnorm(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    const nimcp_gpu_tensor_t* running_mean,
    const nimcp_gpu_tensor_t* running_var,
    const nimcp_gpu_tensor_t* gamma,
    const nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* out,
    float eps)
{
    (void)ctx;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fp16_batchnorm: required parameter is NULL");
        return false;
    }

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;
    const float* rm = running_mean ? (const float*)running_mean->data : NULL;
    const float* rv = running_var ? (const float*)running_var->data : NULL;
    const float* g = gamma ? (const float*)gamma->data : NULL;
    const float* b = beta ? (const float*)beta->data : NULL;

    /* Assume NCHW format: dims[0]=batch, dims[1]=channels */
    size_t channels = (x->ndim >= 2) ? x->dims[1] : 1;
    size_t spatial = 1;
    for (uint32_t d = 2; d < x->ndim; d++) spatial *= x->dims[d];
    size_t batch = x->dims[0];

    for (size_t c = 0; c < channels; c++) {
        float mean_c, var_c;

        if (rm && running_mean && c < running_mean->numel) {
            mean_c = rm[c];
        } else {
            double sum = 0.0;
            size_t count = 0;
            for (size_t n = 0; n < batch; n++) {
                for (size_t s = 0; s < spatial; s++) {
                    sum += (double)src[n * channels * spatial + c * spatial + s];
                    count++;
                }
            }
            mean_c = (count > 0) ? (float)(sum / (double)count) : 0.0f;
        }

        if (rv && running_var && c < running_var->numel) {
            var_c = rv[c];
        } else {
            double var_sum = 0.0;
            size_t count = 0;
            for (size_t n = 0; n < batch; n++) {
                for (size_t s = 0; s < spatial; s++) {
                    double diff = (double)src[n * channels * spatial + c * spatial + s] - (double)mean_c;
                    var_sum += diff * diff;
                    count++;
                }
            }
            var_c = (count > 0) ? (float)(var_sum / (double)count) : 0.0f;
        }

        float inv_std = 1.0f / sqrtf(var_c + eps);
        float gc = (g && gamma && c < gamma->numel) ? g[c] : 1.0f;
        float bc = (b && beta && c < beta->numel) ? b[c] : 0.0f;

        for (size_t n = 0; n < batch; n++) {
            for (size_t s = 0; s < spatial; s++) {
                size_t idx = n * channels * spatial + c * spatial + s;
                if (idx < x->numel && idx < out->numel) {
                    dst[idx] = (src[idx] - mean_c) * inv_std * gc + bc;
                }
            }
        }
    }
    return true;
}

/*=============================================================================
 * Loss Scaling Kernels
 *=============================================================================*/

bool nimcp_fp16_scale_gradients(nimcp_gpu_context_t* ctx, nimcp_gpu_tensor_t* grads, float scale) {
    (void)ctx;
    if (!grads || !grads->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_fp16_scale_gradients: required parameter is NULL");
        return false;
    }
    float* data = (float*)grads->data;
    for (size_t i = 0; i < grads->numel; i++) {
        data[i] *= scale;
    }
    return true;
}

bool nimcp_fp16_check_inf_nan(nimcp_gpu_context_t* ctx, const nimcp_gpu_tensor_t* data, int* found_inf) {
    (void)ctx;
    if (!data || !found_inf || !data->data) {
        return false;
    }
    *found_inf = 0;
    const float* src = (const float*)data->data;
    for (size_t i = 0; i < data->numel; i++) {
        if (isinf(src[i]) || isnan(src[i])) {
            *found_inf = 1;
            return true;
        }
    }
    return true;
}

/*=============================================================================
 * Mixed Precision Optimizer Support
 *=============================================================================*/

bool nimcp_mp_adam_update(
    nimcp_gpu_context_t* ctx,
    nimcp_mp_tensor_t* mp_tensor,
    const nimcp_gpu_tensor_t* gradients,
    nimcp_gpu_tensor_t* m,
    nimcp_gpu_tensor_t* v,
    float lr, float beta1, float beta2, float eps,
    float weight_decay, int step)
{
    (void)ctx;
    if (!mp_tensor || !gradients || !m || !v) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mp_adam_update: required parameter is NULL");
        return false;
    }

    nimcp_gpu_tensor_t* params = mp_tensor->fp32_master ? mp_tensor->fp32_master : mp_tensor->fp16_data;
    if (!params || !params->data || !gradients->data || !m->data || !v->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mp_adam_update: tensor data is NULL");
        return false;
    }

    float* p = (float*)params->data;
    const float* g = (const float*)gradients->data;
    float* m_data = (float*)m->data;
    float* v_data = (float*)v->data;

    /* Bias correction */
    float bc1 = 1.0f - powf(beta1, (float)step);
    float bc2 = 1.0f - powf(beta2, (float)step);

    for (size_t i = 0; i < params->numel && i < gradients->numel; i++) {
        float gi = g[i];

        /* Weight decay (AdamW style) */
        if (weight_decay != 0.0f) {
            p[i] -= lr * weight_decay * p[i];
        }

        /* Update moments */
        m_data[i] = beta1 * m_data[i] + (1.0f - beta1) * gi;
        v_data[i] = beta2 * v_data[i] + (1.0f - beta2) * gi * gi;

        /* Bias-corrected estimates */
        float m_hat = m_data[i] / bc1;
        float v_hat = v_data[i] / bc2;

        /* Parameter update */
        p[i] -= lr * m_hat / (sqrtf(v_hat) + eps);
    }

    /* Sync compute copy if master exists */
    if (mp_tensor->fp32_master && mp_tensor->fp16_data) {
        nimcp_mp_tensor_sync_compute(ctx, mp_tensor);
    }

    return true;
}

bool nimcp_mp_sgd_update(
    nimcp_gpu_context_t* ctx,
    nimcp_mp_tensor_t* mp_tensor,
    const nimcp_gpu_tensor_t* gradients,
    nimcp_gpu_tensor_t* momentum_buffer,
    float lr, float momentum, float weight_decay, bool nesterov)
{
    (void)ctx;
    if (!mp_tensor || !gradients) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mp_sgd_update: required parameter is NULL");
        return false;
    }

    nimcp_gpu_tensor_t* params = mp_tensor->fp32_master ? mp_tensor->fp32_master : mp_tensor->fp16_data;
    if (!params || !params->data || !gradients->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_mp_sgd_update: tensor data is NULL");
        return false;
    }

    float* p = (float*)params->data;
    const float* g_data = (const float*)gradients->data;
    float* mb = momentum_buffer ? (float*)momentum_buffer->data : NULL;

    for (size_t i = 0; i < params->numel && i < gradients->numel; i++) {
        float gi = g_data[i];

        /* Weight decay */
        if (weight_decay != 0.0f) {
            gi += weight_decay * p[i];
        }

        if (mb && momentum != 0.0f) {
            mb[i] = momentum * mb[i] + gi;
            if (nesterov) {
                gi = gi + momentum * mb[i];
            } else {
                gi = mb[i];
            }
        }

        p[i] -= lr * gi;
    }

    if (mp_tensor->fp32_master && mp_tensor->fp16_data) {
        nimcp_mp_tensor_sync_compute(ctx, mp_tensor);
    }

    return true;
}

/*=============================================================================
 * Hardware Capability Detection
 *=============================================================================*/

bool nimcp_tensor_cores_available(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return false;  /* No tensor cores on CPU */
}

bool nimcp_bf16_supported(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return false;  /* No native BF16 on CPU */
}

nimcp_mp_dtype_t nimcp_get_recommended_dtype(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    return MP_DTYPE_FP32;  /* CPU always uses FP32 */
}

void nimcp_amp_get_stats(const nimcp_amp_context_t* ctx, uint64_t* fp16_ops, uint64_t* fp32_ops, uint64_t* tc_ops) {
    if (!ctx) {
        if (fp16_ops) *fp16_ops = 0;
        if (fp32_ops) *fp32_ops = 0;
        if (tc_ops) *tc_ops = 0;
        return;
    }
    if (fp16_ops) *fp16_ops = ctx->fp16_ops;
    if (fp32_ops) *fp32_ops = ctx->fp32_ops;
    if (tc_ops) *tc_ops = ctx->tensor_core_ops;
}

const char* nimcp_mp_dtype_name(nimcp_mp_dtype_t dtype) {
    switch (dtype) {
        case MP_DTYPE_FP32: return "float32";
        case MP_DTYPE_FP16: return "float16";
        case MP_DTYPE_BF16: return "bfloat16";
        case MP_DTYPE_TF32: return "tf32";
        default: return "unknown";
    }
}

size_t nimcp_mp_dtype_size(nimcp_mp_dtype_t dtype) {
    switch (dtype) {
        case MP_DTYPE_FP32: return 4;
        case MP_DTYPE_FP16: return 2;
        case MP_DTYPE_BF16: return 2;
        case MP_DTYPE_TF32: return 4;
        default: return 4;
    }
}

/*=============================================================================
 * AMP Autocast API - CPU fallback
 *=============================================================================*/

nimcp_autocast_ctx_t* nimcp_autocast_create(
    nimcp_gpu_context_t* gpu_ctx,
    nimcp_autocast_mode_t mode)
{
    nimcp_autocast_ctx_t* ctx = nimcp_calloc(1, sizeof(nimcp_autocast_ctx_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_autocast_create: allocation failed");
        return NULL;
    }
    ctx->gpu_ctx = gpu_ctx;
    ctx->mode = mode;
    ctx->enabled = false;
    ctx->nesting_level = 0;

    /* On CPU all ops use FP32 */
    for (int i = 0; i < AUTOCAST_OP_COUNT; i++) {
        ctx->op_dtypes[i] = MP_DTYPE_FP32;
        ctx->op_force_fp32[i] = false;
    }

    return ctx;
}

nimcp_autocast_ctx_t* nimcp_autocast_create_with_config(
    nimcp_gpu_context_t* gpu_ctx,
    const nimcp_autocast_config_t* config)
{
    if (!config) {
        return nimcp_autocast_create(gpu_ctx, AUTOCAST_DISABLED);
    }

    nimcp_autocast_ctx_t* ctx = nimcp_autocast_create(gpu_ctx, config->mode);
    if (!ctx) return NULL;

    if (config->enable_scaler) {
        ctx->scaler = nimcp_loss_scaler_create_custom(
            config->init_scale, MP_DEFAULT_GROWTH_FACTOR,
            MP_DEFAULT_BACKOFF_FACTOR, MP_DEFAULT_GROWTH_INTERVAL, true);
        ctx->owns_scaler = true;
    }

    /* Apply op overrides */
    if (config->op_overrides) {
        for (int i = 0; i < config->num_overrides && i < AUTOCAST_OP_COUNT; i++) {
            ctx->op_dtypes[i] = config->op_overrides[i];
        }
    }

    return ctx;
}

void nimcp_autocast_default_config(nimcp_autocast_config_t* config, nimcp_autocast_mode_t mode) {
    if (!config) return;
    memset(config, 0, sizeof(*config));
    config->mode = mode;
    config->enable_caching = true;
    config->enable_scaler = (mode != AUTOCAST_DISABLED);
    config->init_scale = MP_DEFAULT_INIT_SCALE;
}

void nimcp_autocast_destroy(nimcp_autocast_ctx_t* ctx) {
    if (!ctx) return;
    if (ctx->owns_scaler && ctx->scaler) {
        nimcp_loss_scaler_destroy(ctx->scaler);
    }
    /* Free any cached converted tensors */
    for (int i = 0; i < ctx->cache_count; i++) {
        if (ctx->cache[i].converted && ctx->cache[i].converted != ctx->cache[i].original) {
            nimcp_gpu_tensor_destroy(ctx->cache[i].converted);
        }
    }
    nimcp_free(ctx);
}

bool nimcp_autocast_begin(nimcp_autocast_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_autocast_begin: ctx is NULL");
        return false;
    }
    if (ctx->nesting_level >= AUTOCAST_MAX_NESTING) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_autocast_begin: max nesting exceeded");
        return false;
    }
    ctx->mode_stack[ctx->nesting_level] = ctx->mode;
    ctx->nesting_level++;
    ctx->enabled = true;
    return true;
}

bool nimcp_autocast_begin_with_mode(nimcp_autocast_ctx_t* ctx, nimcp_autocast_mode_t mode) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_autocast_begin_with_mode: ctx is NULL");
        return false;
    }
    if (ctx->nesting_level >= AUTOCAST_MAX_NESTING) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_autocast_begin_with_mode: max nesting exceeded");
        return false;
    }
    ctx->mode_stack[ctx->nesting_level] = ctx->mode;
    ctx->mode = mode;
    ctx->nesting_level++;
    ctx->enabled = true;
    return true;
}

bool nimcp_autocast_end(nimcp_autocast_ctx_t* ctx) {
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_autocast_end: ctx is NULL");
        return false;
    }
    if (ctx->nesting_level <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_autocast_end: not inside autocast block");
        return false;
    }
    ctx->nesting_level--;
    ctx->mode = ctx->mode_stack[ctx->nesting_level];
    ctx->enabled = (ctx->nesting_level > 0);
    return true;
}

bool nimcp_autocast_is_active(const nimcp_autocast_ctx_t* ctx) {
    if (!ctx) return false;
    return ctx->enabled;
}

nimcp_autocast_mode_t nimcp_autocast_get_mode(const nimcp_autocast_ctx_t* ctx) {
    if (!ctx) return AUTOCAST_DISABLED;
    return ctx->mode;
}

nimcp_mp_dtype_t nimcp_autocast_get_op_dtype(const nimcp_autocast_ctx_t* ctx, nimcp_autocast_op_t op) {
    if (!ctx || (int)op >= AUTOCAST_OP_COUNT) return MP_DTYPE_FP32;
    return ctx->op_dtypes[op];
}

bool nimcp_autocast_set_op_dtype(nimcp_autocast_ctx_t* ctx, nimcp_autocast_op_t op, nimcp_mp_dtype_t dtype) {
    if (!ctx || (int)op >= AUTOCAST_OP_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_autocast_set_op_dtype: invalid parameter");
        return false;
    }
    ctx->op_dtypes[op] = dtype;
    return true;
}

void nimcp_autocast_force_fp32(nimcp_autocast_ctx_t* ctx, nimcp_autocast_op_t op, bool force_fp32) {
    if (!ctx || (int)op >= AUTOCAST_OP_COUNT) return;
    ctx->op_force_fp32[op] = force_fp32;
}

nimcp_gpu_tensor_t* nimcp_autocast_cast_input(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* tensor,
    nimcp_autocast_op_t op)
{
    (void)op;
    if (!ctx || !tensor) return tensor;
    ctx->fp32_ops++;
    ctx->casts_performed++;
    return tensor;  /* No casting on CPU */
}

nimcp_gpu_tensor_t* nimcp_autocast_cast_output_fp32(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* tensor)
{
    if (!ctx || !tensor) return tensor;
    return tensor;  /* Already FP32 on CPU */
}

void nimcp_autocast_clear_cache(nimcp_autocast_ctx_t* ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->cache_count; i++) {
        if (ctx->cache[i].converted && ctx->cache[i].converted != ctx->cache[i].original) {
            nimcp_gpu_tensor_destroy(ctx->cache[i].converted);
        }
        ctx->cache[i].valid = false;
    }
    ctx->cache_count = 0;
}

nimcp_gpu_tensor_t* nimcp_autocast_matmul(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* A,
    nimcp_gpu_tensor_t* B,
    nimcp_gpu_tensor_t* C,
    float alpha, float beta,
    bool trans_a, bool trans_b)
{
    if (!ctx || !A || !B || !C) return NULL;
    nimcp_gpu_gemm(ctx->gpu_ctx, A, B, C, alpha, beta, trans_a, trans_b);
    ctx->fp32_ops++;
    return C;
}

nimcp_gpu_tensor_t* nimcp_autocast_softmax(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    if (!ctx || !x || !out) return NULL;
    nimcp_gpu_softmax(ctx->gpu_ctx, x, out);
    ctx->fp32_ops++;
    return out;
}

nimcp_gpu_tensor_t* nimcp_autocast_layernorm(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* gamma,
    nimcp_gpu_tensor_t* beta,
    nimcp_gpu_tensor_t* out,
    float eps)
{
    if (!ctx || !x || !out) return NULL;
    nimcp_fp16_layernorm(ctx->gpu_ctx, x, gamma, beta, out, eps);
    ctx->fp32_ops++;
    return out;
}

nimcp_gpu_tensor_t* nimcp_autocast_activation(
    nimcp_autocast_ctx_t* ctx,
    nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int activation)
{
    if (!ctx || !x || !out) return NULL;
    switch (activation) {
        case 0: nimcp_gpu_relu(ctx->gpu_ctx, x, out); break;
        case 1: nimcp_gpu_gelu(ctx->gpu_ctx, x, out); break;
        case 2: nimcp_gpu_sigmoid(ctx->gpu_ctx, x, out); break;
        case 3: nimcp_gpu_tanh(ctx->gpu_ctx, x, out); break;
        case 4: nimcp_gpu_silu(ctx->gpu_ctx, x, out); break;
        default: nimcp_gpu_relu(ctx->gpu_ctx, x, out); break;
    }
    ctx->fp32_ops++;
    return out;
}

nimcp_loss_scaler_t* nimcp_autocast_get_scaler(nimcp_autocast_ctx_t* ctx) {
    if (!ctx) return NULL;
    return ctx->scaler;
}

void nimcp_autocast_set_scaler(nimcp_autocast_ctx_t* ctx, nimcp_loss_scaler_t* scaler) {
    if (!ctx) return;
    if (ctx->owns_scaler && ctx->scaler) {
        nimcp_loss_scaler_destroy(ctx->scaler);
    }
    ctx->scaler = scaler;
    ctx->owns_scaler = false;
}

float nimcp_autocast_scale_loss(nimcp_autocast_ctx_t* ctx, float loss) {
    if (!ctx || !ctx->scaler) return loss;
    return nimcp_loss_scaler_scale(ctx->scaler, loss);
}

bool nimcp_autocast_unscale_gradients(nimcp_autocast_ctx_t* ctx, nimcp_gpu_tensor_t* gradients) {
    if (!ctx || !ctx->scaler) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_autocast_unscale_gradients: required parameter is NULL");
        return false;
    }
    return nimcp_loss_scaler_unscale(ctx->gpu_ctx, ctx->scaler, gradients);
}

void nimcp_autocast_update_scale(nimcp_autocast_ctx_t* ctx, bool gradients_valid) {
    if (!ctx || !ctx->scaler) return;
    nimcp_loss_scaler_update(ctx->scaler, gradients_valid);
}

void nimcp_autocast_get_stats(
    const nimcp_autocast_ctx_t* ctx,
    uint64_t* casts_performed,
    uint64_t* cache_hits,
    uint64_t* cache_misses)
{
    if (!ctx) {
        if (casts_performed) *casts_performed = 0;
        if (cache_hits) *cache_hits = 0;
        if (cache_misses) *cache_misses = 0;
        return;
    }
    if (casts_performed) *casts_performed = ctx->casts_performed;
    if (cache_hits) *cache_hits = ctx->cache_hits;
    if (cache_misses) *cache_misses = ctx->cache_misses;
}

void nimcp_autocast_reset_stats(nimcp_autocast_ctx_t* ctx) {
    if (!ctx) return;
    ctx->casts_performed = 0;
    ctx->cache_hits = 0;
    ctx->cache_misses = 0;
    ctx->fp16_ops = 0;
    ctx->fp32_ops = 0;
}

const char* nimcp_autocast_op_name(nimcp_autocast_op_t op) {
    switch (op) {
        case AUTOCAST_OP_MATMUL:     return "matmul";
        case AUTOCAST_OP_CONV:       return "conv";
        case AUTOCAST_OP_ATTENTION:  return "attention";
        case AUTOCAST_OP_LINEAR:     return "linear";
        case AUTOCAST_OP_EMBEDDING:  return "embedding";
        case AUTOCAST_OP_SOFTMAX:    return "softmax";
        case AUTOCAST_OP_LAYERNORM:  return "layernorm";
        case AUTOCAST_OP_BATCHNORM:  return "batchnorm";
        case AUTOCAST_OP_GROUPNORM:  return "groupnorm";
        case AUTOCAST_OP_LOSS:       return "loss";
        case AUTOCAST_OP_REDUCTION:  return "reduction";
        case AUTOCAST_OP_ACTIVATION: return "activation";
        case AUTOCAST_OP_ELEMENTWISE: return "elementwise";
        case AUTOCAST_OP_CUSTOM:     return "custom";
        default:                     return "unknown";
    }
}

const char* nimcp_autocast_mode_name(nimcp_autocast_mode_t mode) {
    switch (mode) {
        case AUTOCAST_DISABLED: return "disabled";
        case AUTOCAST_FP16:    return "fp16";
        case AUTOCAST_BF16:    return "bf16";
        case AUTOCAST_TF32:    return "tf32";
        default:               return "unknown";
    }
}

void nimcp_autocast_print_config(const nimcp_autocast_ctx_t* ctx) {
    if (!ctx) return;
    printf("Autocast Config:\n");
    printf("  Mode: %s\n", nimcp_autocast_mode_name(ctx->mode));
    printf("  Enabled: %s\n", ctx->enabled ? "yes" : "no");
    printf("  Nesting: %d\n", ctx->nesting_level);
    printf("  Cache entries: %d\n", ctx->cache_count);
    printf("  Casts: %" PRIu64 ", Hits: %" PRIu64 ", Misses: %" PRIu64 "\n",
           ctx->casts_performed, ctx->cache_hits, ctx->cache_misses);
}
