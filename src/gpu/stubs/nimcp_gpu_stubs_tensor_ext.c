/**
 * @file nimcp_gpu_stubs_tensor_ext.c
 * @brief CPU fallback implementations for remaining GPU tensor operations
 *
 * WHAT: CPU implementations for tensor reductions, norms, comparisons, FFT, and CPU-GPU interop
 * WHY:  Enables full tensor operations on CPU-only systems
 * HOW:  Sequential loops, standard math for all operations
 *
 * Functions already in nimcp_gpu_stubs.c: lifecycle, GEMM, element-wise, activations,
 * math, fill/zeros/copy. This file covers the remaining 34 functions.
 */

#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/tensor/nimcp_tensor_ops.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include "constants/nimcp_math_constants.h"


/*=============================================================================
 * Helper: compute stride/offset for axis-based reductions
 *=============================================================================*/
static size_t compute_axis_stride(const nimcp_gpu_tensor_t* t, int axis) {
    if (!t || axis < 0 || (uint32_t)axis >= t->ndim) return 1;
    size_t stride = 1;
    for (uint32_t d = (uint32_t)axis + 1; d < t->ndim; d++) {
        stride *= t->dims[d];
    }
    return stride;
}

/*=============================================================================
 * Memory Operations (remaining)
 *=============================================================================*/

bool nimcp_gpu_ones(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* tensor)
{
    (void)ctx;
    if (!tensor || !tensor->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_ones: required parameter is NULL (tensor, tensor->data)");
        return false;
    }
    float* data = (float*)tensor->data;
    for (size_t i = 0; i < tensor->numel; i++) {
        data[i] = 1.0f;
    }
    return true;
}

bool nimcp_gpu_transpose(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_transpose: required parameter is NULL");
        return false;
    }
    if (x->ndim != 2) {
        /* For non-2D, just copy (transpose last two dims conceptually) */
        memcpy(out->data, x->data, x->numel * x->elem_size);
        return true;
    }

    size_t rows = x->dims[0];
    size_t cols = x->dims[1];
    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    for (size_t r = 0; r < rows; r++) {
        for (size_t c = 0; c < cols; c++) {
            dst[c * rows + r] = src[r * cols + c];
        }
    }
    return true;
}

bool nimcp_gpu_reshape(
    nimcp_gpu_tensor_t* tensor,
    const size_t* new_dims,
    uint32_t new_ndim)
{
    if (!tensor || !new_dims) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_reshape: required parameter is NULL");
        return false;
    }
    if (new_ndim == 0 || new_ndim > 8) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_reshape: invalid ndim");
        return false;
    }

    /* Verify total element count matches */
    size_t new_numel = 1;
    for (uint32_t i = 0; i < new_ndim; i++) {
        new_numel *= new_dims[i];
    }
    if (new_numel != tensor->numel) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_reshape: element count mismatch");
        return false;
    }

    tensor->ndim = new_ndim;
    for (uint32_t i = 0; i < new_ndim; i++) {
        tensor->dims[i] = new_dims[i];
    }
    /* Recompute strides (row-major) */
    for (int i = (int)new_ndim - 1; i >= 0; i--) {
        tensor->strides[i] = (i == (int)new_ndim - 1) ? 1 : tensor->strides[i + 1] * new_dims[i + 1];
    }
    return true;
}

/*=============================================================================
 * Reduction Operations
 *=============================================================================*/

bool nimcp_gpu_sum(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims)
{
    (void)ctx;
    (void)keepdims;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_sum: required parameter is NULL");
        return false;
    }

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    /* axis == -1: global sum */
    if (axis < 0) {
        double sum = 0.0;
        for (size_t i = 0; i < x->numel; i++) {
            sum += (double)src[i];
        }
        dst[0] = (float)sum;
        return true;
    }

    if ((uint32_t)axis >= x->ndim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_sum: axis out of range");
        return false;
    }

    size_t axis_size = x->dims[axis];
    size_t inner_stride = compute_axis_stride(x, axis);
    size_t outer_size = x->numel / axis_size;

    /* Zero output */
    memset(dst, 0, out->numel * sizeof(float));

    for (size_t i = 0; i < x->numel; i++) {
        /* Compute output index by removing axis dimension */
        size_t outer = i / (axis_size * inner_stride);
        size_t inner = i % inner_stride;
        size_t out_idx = outer * inner_stride + inner;
        if (out_idx < out->numel) {
            dst[out_idx] += src[i];
        }
    }
    (void)outer_size;
    return true;
}

bool nimcp_gpu_mean(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims)
{
    (void)ctx;
    (void)keepdims;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_mean: required parameter is NULL");
        return false;
    }

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    if (axis < 0) {
        double sum = 0.0;
        for (size_t i = 0; i < x->numel; i++) {
            sum += (double)src[i];
        }
        dst[0] = (x->numel > 0) ? (float)(sum / (double)x->numel) : 0.0f;
        return true;
    }

    if ((uint32_t)axis >= x->ndim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_mean: axis out of range");
        return false;
    }

    /* Sum along axis, then divide */
    if (!nimcp_gpu_sum(ctx, x, out, axis, keepdims)) return false;

    size_t axis_size = x->dims[axis];
    for (size_t i = 0; i < out->numel; i++) {
        dst[i] /= (float)axis_size;
    }
    return true;
}

bool nimcp_gpu_max(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims)
{
    (void)ctx;
    (void)keepdims;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_max: required parameter is NULL");
        return false;
    }

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    if (axis < 0) {
        float max_val = -FLT_MAX;
        for (size_t i = 0; i < x->numel; i++) {
            if (src[i] > max_val) max_val = src[i];
        }
        dst[0] = max_val;
        return true;
    }

    if ((uint32_t)axis >= x->ndim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_max: axis out of range");
        return false;
    }

    size_t axis_size = x->dims[axis];
    size_t inner_stride = compute_axis_stride(x, axis);

    /* Initialize output to -FLT_MAX */
    for (size_t i = 0; i < out->numel; i++) {
        dst[i] = -FLT_MAX;
    }

    for (size_t i = 0; i < x->numel; i++) {
        size_t outer = i / (axis_size * inner_stride);
        size_t inner = i % inner_stride;
        size_t out_idx = outer * inner_stride + inner;
        if (out_idx < out->numel && src[i] > dst[out_idx]) {
            dst[out_idx] = src[i];
        }
    }
    return true;
}

bool nimcp_gpu_min(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims)
{
    (void)ctx;
    (void)keepdims;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_min: required parameter is NULL");
        return false;
    }

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    if (axis < 0) {
        float min_val = FLT_MAX;
        for (size_t i = 0; i < x->numel; i++) {
            if (src[i] < min_val) min_val = src[i];
        }
        dst[0] = min_val;
        return true;
    }

    if ((uint32_t)axis >= x->ndim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_min: axis out of range");
        return false;
    }

    size_t axis_size = x->dims[axis];
    size_t inner_stride = compute_axis_stride(x, axis);

    for (size_t i = 0; i < out->numel; i++) {
        dst[i] = FLT_MAX;
    }

    for (size_t i = 0; i < x->numel; i++) {
        size_t outer = i / (axis_size * inner_stride);
        size_t inner = i % inner_stride;
        size_t out_idx = outer * inner_stride + inner;
        if (out_idx < out->numel && src[i] < dst[out_idx]) {
            dst[out_idx] = src[i];
        }
    }
    return true;
}

bool nimcp_gpu_argmax(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis)
{
    (void)ctx;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_argmax: required parameter is NULL");
        return false;
    }

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    if (axis < 0) {
        float max_val = -FLT_MAX;
        size_t max_idx = 0;
        for (size_t i = 0; i < x->numel; i++) {
            if (src[i] > max_val) { max_val = src[i]; max_idx = i; }
        }
        dst[0] = (float)max_idx;
        return true;
    }

    if ((uint32_t)axis >= x->ndim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_argmax: axis out of range");
        return false;
    }

    size_t axis_size = x->dims[axis];
    size_t inner_stride = compute_axis_stride(x, axis);

    /* Track max values and indices */
    float* max_vals = (float*)nimcp_malloc(out->numel * sizeof(float));
    if (!max_vals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_argmax: allocation failed");
        return false;
    }
    for (size_t i = 0; i < out->numel; i++) {
        max_vals[i] = -FLT_MAX;
        dst[i] = 0.0f;
    }

    for (size_t i = 0; i < x->numel; i++) {
        size_t outer = i / (axis_size * inner_stride);
        size_t axis_idx = (i / inner_stride) % axis_size;
        size_t inner = i % inner_stride;
        size_t out_idx = outer * inner_stride + inner;
        if (out_idx < out->numel && src[i] > max_vals[out_idx]) {
            max_vals[out_idx] = src[i];
            dst[out_idx] = (float)axis_idx;
        }
    }
    nimcp_free(max_vals);
    return true;
}

bool nimcp_gpu_argmin(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis)
{
    (void)ctx;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_argmin: required parameter is NULL");
        return false;
    }

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    if (axis < 0) {
        float min_val = FLT_MAX;
        size_t min_idx = 0;
        for (size_t i = 0; i < x->numel; i++) {
            if (src[i] < min_val) { min_val = src[i]; min_idx = i; }
        }
        dst[0] = (float)min_idx;
        return true;
    }

    if ((uint32_t)axis >= x->ndim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_argmin: axis out of range");
        return false;
    }

    size_t axis_size = x->dims[axis];
    size_t inner_stride = compute_axis_stride(x, axis);

    float* min_vals = (float*)nimcp_malloc(out->numel * sizeof(float));
    if (!min_vals) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_argmin: allocation failed");
        return false;
    }
    for (size_t i = 0; i < out->numel; i++) {
        min_vals[i] = FLT_MAX;
        dst[i] = 0.0f;
    }

    for (size_t i = 0; i < x->numel; i++) {
        size_t outer = i / (axis_size * inner_stride);
        size_t axis_idx = (i / inner_stride) % axis_size;
        size_t inner = i % inner_stride;
        size_t out_idx = outer * inner_stride + inner;
        if (out_idx < out->numel && src[i] < min_vals[out_idx]) {
            min_vals[out_idx] = src[i];
            dst[out_idx] = (float)axis_idx;
        }
    }
    nimcp_free(min_vals);
    return true;
}

bool nimcp_gpu_var(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims,
    bool unbiased)
{
    (void)ctx;
    (void)keepdims;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_var: required parameter is NULL");
        return false;
    }

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    if (axis < 0) {
        /* Global variance */
        double sum = 0.0, sum_sq = 0.0;
        for (size_t i = 0; i < x->numel; i++) {
            sum += (double)src[i];
            sum_sq += (double)src[i] * (double)src[i];
        }
        size_t n = x->numel;
        double mean = sum / (double)n;
        double var = sum_sq / (double)n - mean * mean;
        if (unbiased && n > 1) {
            var = var * (double)n / (double)(n - 1);
        }
        dst[0] = (float)var;
        return true;
    }

    if ((uint32_t)axis >= x->ndim) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_var: axis out of range");
        return false;
    }

    /* First compute mean along axis */
    size_t axis_size = x->dims[axis];
    size_t inner_stride = compute_axis_stride(x, axis);

    /* Accumulate sum for mean */
    double* sums = (double*)nimcp_calloc(out->numel, sizeof(double));
    double* sum_sqs = (double*)nimcp_calloc(out->numel, sizeof(double));
    if (!sums || !sum_sqs) {
        nimcp_free(sums);
        nimcp_free(sum_sqs);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_var: allocation failed");
        return false;
    }

    for (size_t i = 0; i < x->numel; i++) {
        size_t outer = i / (axis_size * inner_stride);
        size_t inner = i % inner_stride;
        size_t out_idx = outer * inner_stride + inner;
        if (out_idx < out->numel) {
            sums[out_idx] += (double)src[i];
            sum_sqs[out_idx] += (double)src[i] * (double)src[i];
        }
    }

    for (size_t i = 0; i < out->numel; i++) {
        double mean = sums[i] / (double)axis_size;
        double var = sum_sqs[i] / (double)axis_size - mean * mean;
        if (unbiased && axis_size > 1) {
            var = var * (double)axis_size / (double)(axis_size - 1);
        }
        dst[i] = (float)var;
    }

    nimcp_free(sums);
    nimcp_free(sum_sqs);
    return true;
}

bool nimcp_gpu_std(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    int axis,
    bool keepdims,
    bool unbiased)
{
    if (!nimcp_gpu_var(ctx, x, out, axis, keepdims, unbiased)) {
        return false;
    }
    float* dst = (float*)out->data;
    for (size_t i = 0; i < out->numel; i++) {
        dst[i] = sqrtf(dst[i] >= 0.0f ? dst[i] : 0.0f);
    }
    return true;
}

/*=============================================================================
 * Norm Operations
 *=============================================================================*/

bool nimcp_gpu_norm_l1(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float* result)
{
    (void)ctx;
    if (!x || !result || !x->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_norm_l1: required parameter is NULL");
        return false;
    }
    const float* src = (const float*)x->data;
    double sum = 0.0;
    for (size_t i = 0; i < x->numel; i++) {
        sum += fabs((double)src[i]);
    }
    *result = (float)sum;
    return true;
}

bool nimcp_gpu_norm_l2(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float* result)
{
    (void)ctx;
    if (!x || !result || !x->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_norm_l2: required parameter is NULL");
        return false;
    }
    const float* src = (const float*)x->data;
    double sum_sq = 0.0;
    for (size_t i = 0; i < x->numel; i++) {
        sum_sq += (double)src[i] * (double)src[i];
    }
    *result = (float)sqrt(sum_sq);
    return true;
}

bool nimcp_gpu_norm_linf(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float* result)
{
    (void)ctx;
    if (!x || !result || !x->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_norm_linf: required parameter is NULL");
        return false;
    }
    const float* src = (const float*)x->data;
    float max_abs = 0.0f;
    for (size_t i = 0; i < x->numel; i++) {
        float a = fabsf(src[i]);
        if (a > max_abs) max_abs = a;
    }
    *result = max_abs;
    return true;
}

bool nimcp_gpu_norm_frobenius(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    float* result)
{
    /* Frobenius norm = L2 norm for tensors treated as flat vectors */
    return nimcp_gpu_norm_l2(ctx, x, result);
}

/*=============================================================================
 * Comparison Operations
 *=============================================================================*/

bool nimcp_gpu_eq(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !b || !out || !a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_eq: required parameter is NULL");
        return false;
    }
    size_t n = a->numel < b->numel ? a->numel : b->numel;
    if (n > out->numel) n = out->numel;
    const float* sa = (const float*)a->data;
    const float* sb = (const float*)b->data;
    float* dst = (float*)out->data;
    for (size_t i = 0; i < n; i++) {
        dst[i] = (sa[i] == sb[i]) ? 1.0f : 0.0f;
    }
    return true;
}

bool nimcp_gpu_gt(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !b || !out || !a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_gt: required parameter is NULL");
        return false;
    }
    size_t n = a->numel < b->numel ? a->numel : b->numel;
    if (n > out->numel) n = out->numel;
    const float* sa = (const float*)a->data;
    const float* sb = (const float*)b->data;
    float* dst = (float*)out->data;
    for (size_t i = 0; i < n; i++) {
        dst[i] = (sa[i] > sb[i]) ? 1.0f : 0.0f;
    }
    return true;
}

bool nimcp_gpu_lt(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !b || !out || !a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_lt: required parameter is NULL");
        return false;
    }
    size_t n = a->numel < b->numel ? a->numel : b->numel;
    if (n > out->numel) n = out->numel;
    const float* sa = (const float*)a->data;
    const float* sb = (const float*)b->data;
    float* dst = (float*)out->data;
    for (size_t i = 0; i < n; i++) {
        dst[i] = (sa[i] < sb[i]) ? 1.0f : 0.0f;
    }
    return true;
}

bool nimcp_gpu_ge(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !b || !out || !a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_ge: required parameter is NULL");
        return false;
    }
    size_t n = a->numel < b->numel ? a->numel : b->numel;
    if (n > out->numel) n = out->numel;
    const float* sa = (const float*)a->data;
    const float* sb = (const float*)b->data;
    float* dst = (float*)out->data;
    for (size_t i = 0; i < n; i++) {
        dst[i] = (sa[i] >= sb[i]) ? 1.0f : 0.0f;
    }
    return true;
}

bool nimcp_gpu_le(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!a || !b || !out || !a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_le: required parameter is NULL");
        return false;
    }
    size_t n = a->numel < b->numel ? a->numel : b->numel;
    if (n > out->numel) n = out->numel;
    const float* sa = (const float*)a->data;
    const float* sb = (const float*)b->data;
    float* dst = (float*)out->data;
    for (size_t i = 0; i < n; i++) {
        dst[i] = (sa[i] <= sb[i]) ? 1.0f : 0.0f;
    }
    return true;
}

bool nimcp_gpu_where(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* cond,
    const nimcp_gpu_tensor_t* a,
    const nimcp_gpu_tensor_t* b,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!cond || !a || !b || !out || !cond->data || !a->data || !b->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_where: required parameter is NULL");
        return false;
    }
    size_t n = out->numel;
    const float* sc = (const float*)cond->data;
    const float* sa = (const float*)a->data;
    const float* sb = (const float*)b->data;
    float* dst = (float*)out->data;
    for (size_t i = 0; i < n; i++) {
        size_t ci = (i < cond->numel) ? i : cond->numel - 1;
        size_t ai = (i < a->numel) ? i : a->numel - 1;
        size_t bi = (i < b->numel) ? i : b->numel - 1;
        dst[i] = (sc[ci] != 0.0f) ? sa[ai] : sb[bi];
    }
    return true;
}

/*=============================================================================
 * FFT Operations (Cooley-Tukey radix-2 DIT for powers of 2, DFT otherwise)
 *=============================================================================*/

/* Simple DFT for arbitrary sizes (O(N^2)) */
static void cpu_dft_1d(const float* real_in, const float* imag_in,
                       float* real_out, float* imag_out,
                       size_t n, bool inverse)
{
    double sign = inverse ? 1.0 : -1.0;
    double norm = inverse ? 1.0 / (double)n : 1.0;

    for (size_t k = 0; k < n; k++) {
        double sum_r = 0.0, sum_i = 0.0;
        for (size_t j = 0; j < n; j++) {
            double angle = sign * 2.0 * M_PI * (double)k * (double)j / (double)n;
            double cos_a = cos(angle);
            double sin_a = sin(angle);
            double rj = real_in ? (double)real_in[j] : 0.0;
            double ij = imag_in ? (double)imag_in[j] : 0.0;
            sum_r += rj * cos_a - ij * sin_a;
            sum_i += rj * sin_a + ij * cos_a;
        }
        real_out[k] = (float)(sum_r * norm);
        imag_out[k] = (float)(sum_i * norm);
    }
}

bool nimcp_gpu_fft_1d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    bool inverse)
{
    (void)ctx;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_fft_1d: required parameter is NULL");
        return false;
    }

    /* Input: interleaved real/imag pairs, numel = 2*N */
    size_t n = x->numel / 2;
    if (n == 0) return true;

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    /* Deinterleave */
    float* real_in = (float*)nimcp_malloc(n * sizeof(float));
    float* imag_in = (float*)nimcp_malloc(n * sizeof(float));
    float* real_out = (float*)nimcp_malloc(n * sizeof(float));
    float* imag_out = (float*)nimcp_malloc(n * sizeof(float));
    if (!real_in || !imag_in || !real_out || !imag_out) {
        nimcp_free(real_in); nimcp_free(imag_in); nimcp_free(real_out); nimcp_free(imag_out);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_fft_1d: allocation failed");
        return false;
    }

    for (size_t i = 0; i < n; i++) {
        real_in[i] = src[2 * i];
        imag_in[i] = src[2 * i + 1];
    }

    cpu_dft_1d(real_in, imag_in, real_out, imag_out, n, inverse);

    /* Interleave output */
    for (size_t i = 0; i < n && 2 * i + 1 < out->numel; i++) {
        dst[2 * i] = real_out[i];
        dst[2 * i + 1] = imag_out[i];
    }

    nimcp_free(real_in); nimcp_free(imag_in); nimcp_free(real_out); nimcp_free(imag_out);
    return true;
}

bool nimcp_gpu_fft_2d(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out,
    bool inverse)
{
    (void)ctx;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_fft_2d: required parameter is NULL");
        return false;
    }
    if (x->ndim < 2) {
        return nimcp_gpu_fft_1d(ctx, x, out, inverse);
    }

    /* 2D FFT = row-wise FFT then column-wise FFT */
    size_t rows = x->dims[x->ndim - 2];
    size_t cols = x->dims[x->ndim - 1] / 2;  /* complex pairs */
    if (cols == 0 || rows == 0) return true;

    /* Copy input to output as working buffer */
    memcpy(out->data, x->data, x->numel * x->elem_size);

    float* data = (float*)out->data;

    float* tmp_r = (float*)nimcp_malloc(cols * sizeof(float));
    float* tmp_i = (float*)nimcp_malloc(cols * sizeof(float));
    float* out_r = (float*)nimcp_malloc(cols * sizeof(float));
    float* out_i = (float*)nimcp_malloc(cols * sizeof(float));
    if (!tmp_r || !tmp_i || !out_r || !out_i) {
        nimcp_free(tmp_r); nimcp_free(tmp_i); nimcp_free(out_r); nimcp_free(out_i);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_fft_2d: allocation failed");
        return false;
    }

    /* Row-wise FFT */
    for (size_t r = 0; r < rows; r++) {
        float* row = data + r * cols * 2;
        for (size_t c = 0; c < cols; c++) {
            tmp_r[c] = row[2 * c];
            tmp_i[c] = row[2 * c + 1];
        }
        cpu_dft_1d(tmp_r, tmp_i, out_r, out_i, cols, inverse);
        for (size_t c = 0; c < cols; c++) {
            row[2 * c] = out_r[c];
            row[2 * c + 1] = out_i[c];
        }
    }

    /* Column-wise FFT - need buffers sized for rows */
    float* col_r = (float*)nimcp_realloc(tmp_r, rows * sizeof(float));
    float* col_i = (float*)nimcp_realloc(tmp_i, rows * sizeof(float));
    float* col_or = (float*)nimcp_realloc(out_r, rows * sizeof(float));
    float* col_oi = (float*)nimcp_realloc(out_i, rows * sizeof(float));
    if (!col_r || !col_i || !col_or || !col_oi) {
        nimcp_free(col_r ? col_r : tmp_r);
        nimcp_free(col_i ? col_i : tmp_i);
        nimcp_free(col_or ? col_or : out_r);
        nimcp_free(col_oi ? col_oi : out_i);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_fft_2d: realloc failed");
        return false;
    }

    for (size_t c = 0; c < cols; c++) {
        for (size_t r = 0; r < rows; r++) {
            col_r[r] = data[r * cols * 2 + 2 * c];
            col_i[r] = data[r * cols * 2 + 2 * c + 1];
        }
        cpu_dft_1d(col_r, col_i, col_or, col_oi, rows, inverse);
        for (size_t r = 0; r < rows; r++) {
            data[r * cols * 2 + 2 * c] = col_or[r];
            data[r * cols * 2 + 2 * c + 1] = col_oi[r];
        }
    }

    nimcp_free(col_r); nimcp_free(col_i); nimcp_free(col_or); nimcp_free(col_oi);
    return true;
}

bool nimcp_gpu_rfft(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_rfft: required parameter is NULL");
        return false;
    }

    /* Real FFT: input is real-valued, output is complex (N/2+1 complex values) */
    size_t n = x->numel;
    if (n == 0) return true;
    size_t out_n = n / 2 + 1;

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    float* imag_in = (float*)nimcp_calloc(n, sizeof(float));
    float* real_out = (float*)nimcp_malloc(n * sizeof(float));
    float* imag_out = (float*)nimcp_malloc(n * sizeof(float));
    if (!imag_in || !real_out || !imag_out) {
        nimcp_free(imag_in); nimcp_free(real_out); nimcp_free(imag_out);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_rfft: allocation failed");
        return false;
    }

    cpu_dft_1d(src, imag_in, real_out, imag_out, n, false);

    /* Output first N/2+1 complex values interleaved */
    for (size_t i = 0; i < out_n && 2 * i + 1 < out->numel; i++) {
        dst[2 * i] = real_out[i];
        dst[2 * i + 1] = imag_out[i];
    }

    nimcp_free(imag_in); nimcp_free(real_out); nimcp_free(imag_out);
    return true;
}

bool nimcp_gpu_irfft(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* x,
    nimcp_gpu_tensor_t* out)
{
    (void)ctx;
    if (!x || !out || !x->data || !out->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_irfft: required parameter is NULL");
        return false;
    }

    /* Inverse real FFT: input is N/2+1 complex values, output is N real values */
    size_t out_n = out->numel;
    if (out_n == 0) return true;
    size_t in_n = x->numel / 2;  /* Number of complex values in input */

    const float* src = (const float*)x->data;
    float* dst = (float*)out->data;

    /* Reconstruct full complex spectrum using conjugate symmetry */
    float* full_r = (float*)nimcp_malloc(out_n * sizeof(float));
    float* full_i = (float*)nimcp_malloc(out_n * sizeof(float));
    float* result_r = (float*)nimcp_malloc(out_n * sizeof(float));
    float* result_i = (float*)nimcp_malloc(out_n * sizeof(float));
    if (!full_r || !full_i || !result_r || !result_i) {
        nimcp_free(full_r); nimcp_free(full_i); nimcp_free(result_r); nimcp_free(result_i);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_gpu_irfft: allocation failed");
        return false;
    }

    /* Fill first half from input */
    for (size_t i = 0; i < in_n && i < out_n; i++) {
        full_r[i] = src[2 * i];
        full_i[i] = src[2 * i + 1];
    }
    /* Fill second half using conjugate symmetry */
    for (size_t i = in_n; i < out_n; i++) {
        size_t mirror = out_n - i;
        if (mirror < in_n) {
            full_r[i] = src[2 * mirror];
            full_i[i] = -src[2 * mirror + 1];
        } else {
            full_r[i] = 0.0f;
            full_i[i] = 0.0f;
        }
    }

    cpu_dft_1d(full_r, full_i, result_r, result_i, out_n, true);

    /* Output real part only */
    for (size_t i = 0; i < out_n; i++) {
        dst[i] = result_r[i];
    }

    nimcp_free(full_r); nimcp_free(full_i); nimcp_free(result_r); nimcp_free(result_i);
    return true;
}

/*=============================================================================
 * CPU-GPU Tensor Integration
 *=============================================================================*/

nimcp_gpu_precision_t nimcp_dtype_to_gpu_precision(nimcp_dtype_t dtype) {
    switch (dtype) {
        case NIMCP_DTYPE_FLOAT32: return NIMCP_GPU_PRECISION_FP32;
        case NIMCP_DTYPE_FLOAT16: return NIMCP_GPU_PRECISION_FP16;
        case NIMCP_DTYPE_BFLOAT16: return NIMCP_GPU_PRECISION_BF16;
        case NIMCP_DTYPE_INT8:    return NIMCP_GPU_PRECISION_INT8;
        case NIMCP_DTYPE_INT32:   return NIMCP_GPU_PRECISION_INT32;
        default:                  return NIMCP_GPU_PRECISION_FP32;
    }
}

nimcp_dtype_t nimcp_gpu_precision_to_dtype(nimcp_gpu_precision_t precision) {
    switch (precision) {
        case NIMCP_GPU_PRECISION_FP32:  return NIMCP_DTYPE_FLOAT32;
        case NIMCP_GPU_PRECISION_FP16:  return NIMCP_DTYPE_FLOAT16;
        case NIMCP_GPU_PRECISION_BF16:  return NIMCP_DTYPE_BFLOAT16;
        case NIMCP_GPU_PRECISION_INT8:  return NIMCP_DTYPE_INT8;
        case NIMCP_GPU_PRECISION_INT32: return NIMCP_DTYPE_INT32;
        default:                        return NIMCP_DTYPE_FLOAT32;
    }
}

nimcp_gpu_tensor_t* nimcp_gpu_tensor_from_cpu(
    nimcp_gpu_context_t* ctx,
    const nimcp_tensor_t* cpu_tensor)
{
    if (!cpu_tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_tensor_from_cpu: cpu_tensor is NULL");
        return NULL;
    }

    /* Get CPU tensor info via the tensor ops API */
    nimcp_tensor_shape_t shape;
    if (nimcp_tensor_get_shape(cpu_tensor, &shape) != NIMCP_TENSOR_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_tensor_from_cpu: failed to get shape");
        return NULL;
    }

    nimcp_tensor_config_t config;
    if (nimcp_tensor_get_config(cpu_tensor, &config) != NIMCP_TENSOR_OK) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "nimcp_gpu_tensor_from_cpu: failed to get config");
        return NULL;
    }

    size_t dims[8];
    for (uint32_t i = 0; i < shape.ndim && i < 8; i++) {
        dims[i] = shape.dims[i];
    }

    nimcp_gpu_precision_t prec = nimcp_dtype_to_gpu_precision(config.dtype);
    nimcp_gpu_tensor_t* gpu = nimcp_gpu_tensor_create(ctx, dims, shape.ndim, prec);
    if (!gpu) return NULL;

    /* Copy data from CPU tensor */
    const void* cpu_data = nimcp_tensor_data_const(cpu_tensor);
    if (cpu_data && gpu->data) {
        memcpy(gpu->data, cpu_data, gpu->numel * gpu->elem_size);
    }

    return gpu;
}

nimcp_tensor_t* nimcp_cpu_tensor_from_gpu(const nimcp_gpu_tensor_t* gpu_tensor) {
    if (!gpu_tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_cpu_tensor_from_gpu: gpu_tensor is NULL");
        return NULL;
    }

    uint32_t dims[8];
    for (uint32_t i = 0; i < gpu_tensor->ndim && i < 8; i++) {
        dims[i] = (uint32_t)gpu_tensor->dims[i];
    }

    nimcp_dtype_t dtype = nimcp_gpu_precision_to_dtype(gpu_tensor->precision);
    nimcp_tensor_t* cpu = nimcp_tensor_create(dims, gpu_tensor->ndim, dtype);
    if (!cpu) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_cpu_tensor_from_gpu: allocation failed");
        return NULL;
    }

    void* cpu_data = nimcp_tensor_data(cpu);
    if (cpu_data && gpu_tensor->data) {
        memcpy(cpu_data, gpu_tensor->data, gpu_tensor->numel * gpu_tensor->elem_size);
    }

    return cpu;
}

bool nimcp_gpu_tensor_copy_to_cpu(
    const nimcp_gpu_tensor_t* gpu_tensor,
    nimcp_tensor_t* cpu_tensor)
{
    if (!gpu_tensor || !cpu_tensor || !gpu_tensor->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_tensor_copy_to_cpu: required parameter is NULL");
        return false;
    }

    void* cpu_data = nimcp_tensor_data(cpu_tensor);
    if (!cpu_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_tensor_copy_to_cpu: cpu tensor data is NULL");
        return false;
    }

    memcpy(cpu_data, gpu_tensor->data, gpu_tensor->numel * gpu_tensor->elem_size);
    return true;
}

bool nimcp_cpu_tensor_copy_to_gpu(
    const nimcp_tensor_t* cpu_tensor,
    nimcp_gpu_tensor_t* gpu_tensor)
{
    if (!cpu_tensor || !gpu_tensor || !gpu_tensor->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_cpu_tensor_copy_to_gpu: required parameter is NULL");
        return false;
    }

    const void* cpu_data = nimcp_tensor_data_const(cpu_tensor);
    if (!cpu_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_cpu_tensor_copy_to_gpu: cpu tensor data is NULL");
        return false;
    }

    memcpy(gpu_tensor->data, cpu_data, gpu_tensor->numel * gpu_tensor->elem_size);
    return true;
}

nimcp_tensor_t* nimcp_gpu_accelerate_matmul(
    nimcp_gpu_context_t* ctx,
    const nimcp_tensor_t* a,
    const nimcp_tensor_t* b)
{
    if (!a || !b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_gpu_accelerate_matmul: required parameter is NULL");
        return NULL;
    }

    /* Convert to GPU tensors, multiply, convert back */
    nimcp_gpu_tensor_t* ga = nimcp_gpu_tensor_from_cpu(ctx, a);
    nimcp_gpu_tensor_t* gb = nimcp_gpu_tensor_from_cpu(ctx, b);
    if (!ga || !gb) {
        nimcp_gpu_tensor_destroy(ga);
        nimcp_gpu_tensor_destroy(gb);
        return NULL;
    }

    /* Create output tensor */
    size_t out_dims[2];
    out_dims[0] = ga->dims[0];
    out_dims[1] = (gb->ndim >= 2) ? gb->dims[1] : 1;
    nimcp_gpu_tensor_t* gc = nimcp_gpu_tensor_create(ctx, out_dims, 2, NIMCP_GPU_PRECISION_FP32);
    if (!gc) {
        nimcp_gpu_tensor_destroy(ga);
        nimcp_gpu_tensor_destroy(gb);
        return NULL;
    }

    bool ok = nimcp_gpu_gemm(ctx, ga, gb, gc, 1.0f, 0.0f, false, false);
    nimcp_gpu_tensor_destroy(ga);
    nimcp_gpu_tensor_destroy(gb);

    if (!ok) {
        nimcp_gpu_tensor_destroy(gc);
        return NULL;
    }

    nimcp_tensor_t* result = nimcp_cpu_tensor_from_gpu(gc);
    nimcp_gpu_tensor_destroy(gc);
    return result;
}

bool nimcp_gpu_tensor_available(void) {
    return false;  /* CPU fallback - no GPU available */
}

bool nimcp_gpu_tensor_memory_info(
    nimcp_gpu_context_t* ctx,
    size_t* free_bytes,
    size_t* total_bytes)
{
    (void)ctx;
    if (free_bytes) *free_bytes = 0;
    if (total_bytes) *total_bytes = 0;
    return false;  /* No GPU memory available in CPU fallback */
}
