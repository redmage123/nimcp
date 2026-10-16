/**
 * @file nimcp_gpu_stubs_int8.c
 * @brief CPU fallback implementations for INT8 quantization inference functions
 *
 * WHAT: CPU implementations for all 48 functions declared in nimcp_int8_inference.h
 * WHY:  Enables INT8 quantization testing on CPU-only systems without CUDA
 * HOW:  Scalar loops for quantize/dequantize, naive GEMM, im2col+GEMM for conv2d
 *
 * @author NIMCP Development Team
 * @date 2026-02-12
 */

#include "gpu/inference/nimcp_int8_inference.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/*=============================================================================
 * Internal Helpers
 *=============================================================================*/

/**
 * @brief Clamp an integer value to INT8 range [-128, 127]
 */
static inline int32_t clamp_int8(int32_t val) {
    if (val < INT8_QUANT_MIN) return INT8_QUANT_MIN;
    if (val > INT8_QUANT_MAX) return INT8_QUANT_MAX;
    return val;
}

/**
 * @brief Quantize a single FP32 value to INT8
 *
 * q = clamp(round(x / scale) + zero_point, -128, 127)
 */
static inline int8_t quantize_scalar(float x, float scale, int32_t zero_point) {
    if (scale < INT8_SCALE_MIN) scale = INT8_SCALE_MIN;
    int32_t q = (int32_t)roundf(x / scale) + zero_point;
    return (int8_t)clamp_int8(q);
}

/**
 * @brief Dequantize a single INT8 value to FP32
 *
 * x = (q - zero_point) * scale
 */
static inline float dequantize_scalar(int8_t q, float scale, int32_t zero_point) {
    return ((float)q - (float)zero_point) * scale;
}

/*=============================================================================
 * Quantization Parameters API (5 functions)
 *=============================================================================*/

/* 1. nimcp_int8_params_init */
int nimcp_int8_params_init(nimcp_int8_quant_params_t* params) {
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_params_init: params is NULL");
        return -1;
    }

    memset(params, 0, sizeof(*params));
    params->scale = 1.0f;
    params->zero_point = 0;
    params->min_val = 0.0f;
    params->max_val = 0.0f;
    params->symmetric = true;
    params->granularity = INT8_GRANULARITY_TENSOR;
    params->num_channels = 0;
    params->channel_scales = NULL;
    params->channel_zero_points = NULL;
    params->group_size = 0;
    params->num_groups = 0;
    params->group_scales = NULL;
    params->group_zero_points = NULL;

    return 0;
}

/* 2. nimcp_int8_params_create_per_channel */
nimcp_int8_quant_params_t* nimcp_int8_params_create_per_channel(int num_channels) {
    if (num_channels <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_params_create_per_channel: num_channels <= 0");
        return NULL;
    }

    nimcp_int8_quant_params_t* params = nimcp_calloc(1, sizeof(nimcp_int8_quant_params_t));
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_params_create_per_channel: allocation failed");
        return NULL;
    }

    params->scale = 1.0f;
    params->zero_point = 0;
    params->symmetric = true;
    params->granularity = INT8_GRANULARITY_CHANNEL;
    params->num_channels = num_channels;

    params->channel_scales = nimcp_calloc((size_t)num_channels, sizeof(float));
    params->channel_zero_points = nimcp_calloc((size_t)num_channels, sizeof(int32_t));

    if (!params->channel_scales || !params->channel_zero_points) {
        nimcp_free(params->channel_scales);
        nimcp_free(params->channel_zero_points);
        nimcp_free(params);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_params_create_per_channel: channel array allocation failed");
        return NULL;
    }

    /* Initialize scales to 1.0 */
    for (int i = 0; i < num_channels; i++) {
        params->channel_scales[i] = 1.0f;
    }

    return params;
}

/* 3. nimcp_int8_params_create_per_group */
nimcp_int8_quant_params_t* nimcp_int8_params_create_per_group(
    int num_elements,
    int group_size)
{
    if (num_elements <= 0 || group_size <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_params_create_per_group: invalid num_elements or group_size");
        return NULL;
    }

    int num_groups = (num_elements + group_size - 1) / group_size;

    nimcp_int8_quant_params_t* params = nimcp_calloc(1, sizeof(nimcp_int8_quant_params_t));
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_params_create_per_group: allocation failed");
        return NULL;
    }

    params->scale = 1.0f;
    params->zero_point = 0;
    params->symmetric = true;
    params->granularity = INT8_GRANULARITY_GROUP;
    params->group_size = group_size;
    params->num_groups = num_groups;

    params->group_scales = nimcp_calloc((size_t)num_groups, sizeof(float));
    params->group_zero_points = nimcp_calloc((size_t)num_groups, sizeof(int32_t));

    if (!params->group_scales || !params->group_zero_points) {
        nimcp_free(params->group_scales);
        nimcp_free(params->group_zero_points);
        nimcp_free(params);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_params_create_per_group: group array allocation failed");
        return NULL;
    }

    /* Initialize scales to 1.0 */
    for (int i = 0; i < num_groups; i++) {
        params->group_scales[i] = 1.0f;
    }

    return params;
}

/* 4. nimcp_int8_params_destroy */
void nimcp_int8_params_destroy(nimcp_int8_quant_params_t* params) {
    if (!params) return;

    nimcp_free(params->channel_scales);
    nimcp_free(params->channel_zero_points);
    nimcp_free(params->group_scales);
    nimcp_free(params->group_zero_points);
    nimcp_free(params);
}

/* 5. nimcp_int8_compute_params_from_minmax */
int nimcp_int8_compute_params_from_minmax(
    float min_val,
    float max_val,
    bool symmetric,
    nimcp_int8_quant_params_t* params)
{
    if (!params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_compute_params_from_minmax: params is NULL");
        return -1;
    }

    params->min_val = min_val;
    params->max_val = max_val;
    params->symmetric = symmetric;

    if (symmetric) {
        /* Symmetric: range = [-absmax, absmax], zero_point = 0 */
        float absmax = fmaxf(fabsf(min_val), fabsf(max_val));
        if (absmax < INT8_SCALE_MIN) absmax = INT8_SCALE_MIN;
        params->scale = absmax / 127.0f;
        params->zero_point = 0;
    } else {
        /* Asymmetric: map [min_val, max_val] to [-128, 127] */
        float range = max_val - min_val;
        if (range < INT8_SCALE_MIN) range = INT8_SCALE_MIN;
        params->scale = range / 255.0f;
        params->zero_point = (int32_t)roundf(-min_val / params->scale) + INT8_QUANT_MIN;
        /* Clamp zero_point to valid INT8 range */
        if (params->zero_point < INT8_QUANT_MIN) params->zero_point = INT8_QUANT_MIN;
        if (params->zero_point > INT8_QUANT_MAX) params->zero_point = INT8_QUANT_MAX;
    }

    if (params->scale < INT8_SCALE_MIN) {
        params->scale = INT8_SCALE_MIN;
    }

    return 0;
}

/*=============================================================================
 * INT8 Tensor API (5 functions)
 *=============================================================================*/

/* 6. nimcp_int8_tensor_create */
nimcp_int8_tensor_t* nimcp_int8_tensor_create(
    nimcp_gpu_context_t* ctx,
    const size_t* dims,
    size_t rank)
{
    (void)ctx;

    if (!dims || rank == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_tensor_create: dims is NULL or rank is 0");
        return NULL;
    }

    nimcp_int8_tensor_t* tensor = nimcp_calloc(1, sizeof(nimcp_int8_tensor_t));
    if (!tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_tensor_create: allocation failed");
        return NULL;
    }

    tensor->rank = rank;
    tensor->ctx = ctx;
    tensor->owns_data = true;

    /* Allocate dims */
    tensor->dims = nimcp_malloc(rank * sizeof(size_t));
    if (!tensor->dims) {
        nimcp_free(tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_tensor_create: dims allocation failed");
        return NULL;
    }

    /* Compute total elements */
    tensor->numel = 1;
    for (size_t i = 0; i < rank; i++) {
        tensor->dims[i] = dims[i];
        tensor->numel *= dims[i];
    }

    /* Allocate INT8 data buffer */
    tensor->data = nimcp_calloc(tensor->numel, sizeof(int8_t));
    if (!tensor->data) {
        nimcp_free(tensor->dims);
        nimcp_free(tensor);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_tensor_create: data allocation failed");
        return NULL;
    }

    /* Initialize default quant params */
    nimcp_int8_params_init(&tensor->params);

    return tensor;
}

/* 7. nimcp_int8_tensor_from_fp32 */
nimcp_int8_tensor_t* nimcp_int8_tensor_from_fp32(
    const nimcp_gpu_tensor_t* fp32_tensor,
    const nimcp_int8_quant_params_t* params)
{
    if (!fp32_tensor || !fp32_tensor->data || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_tensor_from_fp32: required parameter is NULL");
        return NULL;
    }

    nimcp_int8_tensor_t* tensor = nimcp_int8_tensor_create(
        fp32_tensor->ctx, fp32_tensor->dims, (size_t)fp32_tensor->ndim);
    if (!tensor) return NULL;

    /* Copy quantization parameters */
    tensor->params = *params;
    /* Null out per-channel/per-group pointers (shallow copy); we don't own them */
    tensor->params.channel_scales = NULL;
    tensor->params.channel_zero_points = NULL;
    tensor->params.group_scales = NULL;
    tensor->params.group_zero_points = NULL;

    /* Deep copy per-channel arrays if needed */
    if (params->granularity == INT8_GRANULARITY_CHANNEL && params->num_channels > 0) {
        tensor->params.num_channels = params->num_channels;
        if (params->channel_scales) {
            tensor->params.channel_scales = nimcp_malloc(
                (size_t)params->num_channels * sizeof(float));
            if (tensor->params.channel_scales) {
                memcpy(tensor->params.channel_scales, params->channel_scales,
                       (size_t)params->num_channels * sizeof(float));
            }
        }
        if (params->channel_zero_points) {
            tensor->params.channel_zero_points = nimcp_malloc(
                (size_t)params->num_channels * sizeof(int32_t));
            if (tensor->params.channel_zero_points) {
                memcpy(tensor->params.channel_zero_points, params->channel_zero_points,
                       (size_t)params->num_channels * sizeof(int32_t));
            }
        }
    }

    /* Quantize FP32 -> INT8 */
    const float* fp32_data = (const float*)fp32_tensor->data;
    for (size_t i = 0; i < tensor->numel; i++) {
        tensor->data[i] = quantize_scalar(fp32_data[i], params->scale, params->zero_point);
    }

    return tensor;
}

/* 8. nimcp_int8_tensor_to_fp32 */
nimcp_gpu_tensor_t* nimcp_int8_tensor_to_fp32(const nimcp_int8_tensor_t* int8_tensor) {
    if (!int8_tensor || !int8_tensor->data || !int8_tensor->dims) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_tensor_to_fp32: required parameter is NULL");
        return NULL;
    }

    nimcp_gpu_tensor_t* fp32_tensor = nimcp_gpu_tensor_create(
        int8_tensor->ctx, int8_tensor->dims, (uint32_t)int8_tensor->rank,
        NIMCP_GPU_PRECISION_FP32);
    if (!fp32_tensor) return NULL;

    /* Dequantize INT8 -> FP32 */
    float* fp32_data = (float*)fp32_tensor->data;
    const nimcp_int8_quant_params_t* p = &int8_tensor->params;

    for (size_t i = 0; i < int8_tensor->numel; i++) {
        fp32_data[i] = dequantize_scalar(int8_tensor->data[i], p->scale, p->zero_point);
    }

    return fp32_tensor;
}

/* 9. nimcp_int8_tensor_destroy */
void nimcp_int8_tensor_destroy(nimcp_int8_tensor_t* tensor) {
    if (!tensor) return;

    if (tensor->owns_data) {
        nimcp_free(tensor->data);
    }
    nimcp_free(tensor->dims);

    /* Free per-channel/per-group arrays owned by the tensor's params copy */
    nimcp_free(tensor->params.channel_scales);
    nimcp_free(tensor->params.channel_zero_points);
    nimcp_free(tensor->params.group_scales);
    nimcp_free(tensor->params.group_zero_points);

    nimcp_free(tensor);
}

/* 10. nimcp_int8_tensor_clone */
nimcp_int8_tensor_t* nimcp_int8_tensor_clone(const nimcp_int8_tensor_t* tensor) {
    if (!tensor || !tensor->data || !tensor->dims) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_tensor_clone: required parameter is NULL");
        return NULL;
    }

    nimcp_int8_tensor_t* clone = nimcp_int8_tensor_create(
        tensor->ctx, tensor->dims, tensor->rank);
    if (!clone) return NULL;

    /* Copy data */
    memcpy(clone->data, tensor->data, tensor->numel * sizeof(int8_t));

    /* Copy quant params (scalar fields) */
    clone->params = tensor->params;
    clone->params.channel_scales = NULL;
    clone->params.channel_zero_points = NULL;
    clone->params.group_scales = NULL;
    clone->params.group_zero_points = NULL;

    /* Deep copy per-channel arrays */
    if (tensor->params.granularity == INT8_GRANULARITY_CHANNEL &&
        tensor->params.num_channels > 0) {
        clone->params.num_channels = tensor->params.num_channels;
        if (tensor->params.channel_scales) {
            clone->params.channel_scales = nimcp_malloc(
                (size_t)tensor->params.num_channels * sizeof(float));
            if (clone->params.channel_scales) {
                memcpy(clone->params.channel_scales, tensor->params.channel_scales,
                       (size_t)tensor->params.num_channels * sizeof(float));
            }
        }
        if (tensor->params.channel_zero_points) {
            clone->params.channel_zero_points = nimcp_malloc(
                (size_t)tensor->params.num_channels * sizeof(int32_t));
            if (clone->params.channel_zero_points) {
                memcpy(clone->params.channel_zero_points,
                       tensor->params.channel_zero_points,
                       (size_t)tensor->params.num_channels * sizeof(int32_t));
            }
        }
    }

    /* Deep copy per-group arrays */
    if (tensor->params.granularity == INT8_GRANULARITY_GROUP &&
        tensor->params.num_groups > 0) {
        clone->params.num_groups = tensor->params.num_groups;
        clone->params.group_size = tensor->params.group_size;
        if (tensor->params.group_scales) {
            clone->params.group_scales = nimcp_malloc(
                (size_t)tensor->params.num_groups * sizeof(float));
            if (clone->params.group_scales) {
                memcpy(clone->params.group_scales, tensor->params.group_scales,
                       (size_t)tensor->params.num_groups * sizeof(float));
            }
        }
        if (tensor->params.group_zero_points) {
            clone->params.group_zero_points = nimcp_malloc(
                (size_t)tensor->params.num_groups * sizeof(int32_t));
            if (clone->params.group_zero_points) {
                memcpy(clone->params.group_zero_points,
                       tensor->params.group_zero_points,
                       (size_t)tensor->params.num_groups * sizeof(int32_t));
            }
        }
    }

    return clone;
}

/*=============================================================================
 * Calibrator API (8 functions)
 *=============================================================================*/

/* 11. nimcp_int8_calibrator_create */
nimcp_int8_calibrator_t* nimcp_int8_calibrator_create(
    nimcp_gpu_context_t* ctx,
    nimcp_int8_calib_method_t method,
    bool per_channel,
    int num_channels,
    int num_bins)
{
    (void)ctx;

    if (per_channel && num_channels <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_calibrator_create: per_channel requires num_channels > 0");
        return NULL;
    }

    nimcp_int8_calibrator_t* cal = nimcp_calloc(1, sizeof(nimcp_int8_calibrator_t));
    if (!cal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_calibrator_create: allocation failed");
        return NULL;
    }

    cal->ctx = ctx;
    cal->method = method;
    cal->per_channel = per_channel;
    cal->num_channels = per_channel ? num_channels : 1;
    cal->scheme = INT8_SCHEME_SYMMETRIC;
    cal->target_samples = INT8_CALIBRATION_DEFAULT_SAMPLES;
    cal->num_samples = 0;
    cal->calibration_complete = false;
    cal->percentile = 99.99f;

    /* Allocate running min/max */
    int stat_count = cal->num_channels;
    cal->running_min = nimcp_malloc((size_t)stat_count * sizeof(float));
    cal->running_max = nimcp_malloc((size_t)stat_count * sizeof(float));

    if (!cal->running_min || !cal->running_max) {
        nimcp_int8_calibrator_destroy(cal);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_calibrator_create: min/max allocation failed");
        return NULL;
    }

    /* Initialize min/max to extreme values */
    for (int i = 0; i < stat_count; i++) {
        cal->running_min[i] = FLT_MAX;
        cal->running_max[i] = -FLT_MAX;
    }

    /* Allocate histogram for histogram-based methods */
    if (method == INT8_CALIB_HISTOGRAM || method == INT8_CALIB_ENTROPY ||
        method == INT8_CALIB_PERCENTILE || method == INT8_CALIB_MSE) {
        cal->num_bins = (num_bins > 0) ? num_bins : INT8_CALIBRATION_DEFAULT_BINS;
        cal->histogram = nimcp_calloc((size_t)cal->num_bins, sizeof(int));
        if (!cal->histogram) {
            nimcp_int8_calibrator_destroy(cal);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_int8_calibrator_create: histogram allocation failed");
            return NULL;
        }
        cal->hist_min = FLT_MAX;
        cal->hist_max = -FLT_MAX;
        cal->bin_width = 0.0f;
    }

    /* No device buffers in CPU fallback */
    cal->d_running_min = NULL;
    cal->d_running_max = NULL;
    cal->d_histogram = NULL;

    return cal;
}

/* 12. nimcp_int8_calibrator_destroy */
void nimcp_int8_calibrator_destroy(nimcp_int8_calibrator_t* cal) {
    if (!cal) return;

    nimcp_free(cal->running_min);
    nimcp_free(cal->running_max);
    nimcp_free(cal->histogram);
    /* Device buffers are not allocated in CPU fallback */
    nimcp_free(cal);
}

/* 13. nimcp_int8_calibrator_reset */
void nimcp_int8_calibrator_reset(nimcp_int8_calibrator_t* cal) {
    if (!cal) return;

    cal->num_samples = 0;
    cal->calibration_complete = false;

    int stat_count = cal->num_channels;
    for (int i = 0; i < stat_count; i++) {
        cal->running_min[i] = FLT_MAX;
        cal->running_max[i] = -FLT_MAX;
    }

    if (cal->histogram) {
        memset(cal->histogram, 0, (size_t)cal->num_bins * sizeof(int));
        cal->hist_min = FLT_MAX;
        cal->hist_max = -FLT_MAX;
        cal->bin_width = 0.0f;
    }
}

/* 14. nimcp_int8_calibrator_observe */
int nimcp_int8_calibrator_observe(
    nimcp_int8_calibrator_t* cal,
    const float* data,
    size_t numel)
{
    if (!cal || !data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_calibrator_observe: required parameter is NULL");
        return -1;
    }

    if (numel == 0) return 0;

    if (cal->per_channel && cal->num_channels > 1) {
        /* Per-channel: data is assumed [N, C, ...], track min/max per channel */
        size_t elements_per_channel = numel / (size_t)cal->num_channels;
        if (elements_per_channel == 0) elements_per_channel = 1;

        for (size_t i = 0; i < numel; i++) {
            int ch = (int)((i / elements_per_channel) % (size_t)cal->num_channels);
            if (ch >= cal->num_channels) ch = cal->num_channels - 1;
            float val = data[i];
            if (val < cal->running_min[ch]) cal->running_min[ch] = val;
            if (val > cal->running_max[ch]) cal->running_max[ch] = val;
        }
    } else {
        /* Per-tensor: single min/max */
        for (size_t i = 0; i < numel; i++) {
            float val = data[i];
            if (val < cal->running_min[0]) cal->running_min[0] = val;
            if (val > cal->running_max[0]) cal->running_max[0] = val;
        }
    }

    /* Update histogram if applicable */
    if (cal->histogram) {
        /* First pass: update global histogram range from running min/max */
        float gmin = cal->running_min[0];
        float gmax = cal->running_max[0];
        for (int ch = 1; ch < cal->num_channels; ch++) {
            if (cal->running_min[ch] < gmin) gmin = cal->running_min[ch];
            if (cal->running_max[ch] > gmax) gmax = cal->running_max[ch];
        }

        /* Re-build histogram if range expanded significantly */
        if (gmin < cal->hist_min || gmax > cal->hist_max) {
            cal->hist_min = gmin;
            cal->hist_max = gmax;
            float range = gmax - gmin;
            if (range < INT8_SCALE_MIN) range = INT8_SCALE_MIN;
            cal->bin_width = range / (float)cal->num_bins;
            /* Clear and re-bin (simple approach) */
            memset(cal->histogram, 0, (size_t)cal->num_bins * sizeof(int));
        }

        if (cal->bin_width > 0.0f) {
            for (size_t i = 0; i < numel; i++) {
                int bin = (int)((data[i] - cal->hist_min) / cal->bin_width);
                if (bin < 0) bin = 0;
                if (bin >= cal->num_bins) bin = cal->num_bins - 1;
                cal->histogram[bin]++;
            }
        }
    }

    cal->num_samples++;
    if (cal->num_samples >= cal->target_samples) {
        cal->calibration_complete = true;
    }

    return 0;
}

/* 15. nimcp_int8_calibrator_observe_tensor */
int nimcp_int8_calibrator_observe_tensor(
    nimcp_int8_calibrator_t* cal,
    const nimcp_gpu_tensor_t* tensor)
{
    if (!cal || !tensor || !tensor->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_calibrator_observe_tensor: required parameter is NULL");
        return -1;
    }

    /* In CPU fallback, GPU tensor data is just host memory */
    return nimcp_int8_calibrator_observe(cal, (const float*)tensor->data, tensor->numel);
}

/* 16. nimcp_int8_calibrator_compute_params */
int nimcp_int8_calibrator_compute_params(
    nimcp_int8_calibrator_t* cal,
    nimcp_int8_quant_params_t* params)
{
    if (!cal || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_calibrator_compute_params: required parameter is NULL");
        return -1;
    }

    if (cal->num_samples == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_calibrator_compute_params: no samples observed");
        return -1;
    }

    bool symmetric = (cal->scheme == INT8_SCHEME_SYMMETRIC);

    if (cal->per_channel && cal->num_channels > 1) {
        params->granularity = INT8_GRANULARITY_CHANNEL;
        params->num_channels = cal->num_channels;
        params->symmetric = symmetric;

        /* Allocate per-channel arrays if not already allocated */
        if (!params->channel_scales) {
            params->channel_scales = nimcp_malloc(
                (size_t)cal->num_channels * sizeof(float));
        }
        if (!params->channel_zero_points) {
            params->channel_zero_points = nimcp_calloc(
                (size_t)cal->num_channels, sizeof(int32_t));
        }
        if (!params->channel_scales || !params->channel_zero_points) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                "nimcp_int8_calibrator_compute_params: channel array allocation failed");
            return -1;
        }

        for (int ch = 0; ch < cal->num_channels; ch++) {
            nimcp_int8_quant_params_t ch_params;
            nimcp_int8_params_init(&ch_params);
            nimcp_int8_compute_params_from_minmax(
                cal->running_min[ch], cal->running_max[ch], symmetric, &ch_params);
            params->channel_scales[ch] = ch_params.scale;
            params->channel_zero_points[ch] = ch_params.zero_point;
        }

        /* Set global params from overall min/max */
        params->min_val = cal->running_min[0];
        params->max_val = cal->running_max[0];
        for (int ch = 1; ch < cal->num_channels; ch++) {
            if (cal->running_min[ch] < params->min_val)
                params->min_val = cal->running_min[ch];
            if (cal->running_max[ch] > params->max_val)
                params->max_val = cal->running_max[ch];
        }
        nimcp_int8_compute_params_from_minmax(
            params->min_val, params->max_val, symmetric, params);
        /* Restore granularity after compute_params_from_minmax overwrites it */
        params->granularity = INT8_GRANULARITY_CHANNEL;
        params->num_channels = cal->num_channels;
    } else {
        /* Per-tensor calibration */
        return nimcp_int8_compute_params_from_minmax(
            cal->running_min[0], cal->running_max[0], symmetric, params);
    }

    return 0;
}

/* 17. nimcp_int8_calibrator_compute_entropy */
int nimcp_int8_calibrator_compute_entropy(
    nimcp_int8_calibrator_t* cal,
    nimcp_int8_quant_params_t* params)
{
    if (!cal || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_calibrator_compute_entropy: required parameter is NULL");
        return -1;
    }

    if (!cal->histogram || cal->num_bins == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_calibrator_compute_entropy: no histogram data");
        return -1;
    }

    /*
     * Simplified entropy-based calibration:
     * Try different thresholds (bin indices) and find the one that minimizes
     * KL-divergence between the original distribution and the quantized version.
     * CPU fallback uses a simpler heuristic: find the threshold that covers
     * the most mass with the least error.
     */
    int total_count = 0;
    for (int i = 0; i < cal->num_bins; i++) {
        total_count += cal->histogram[i];
    }

    if (total_count == 0) {
        return nimcp_int8_compute_params_from_minmax(
            cal->hist_min, cal->hist_max, true, params);
    }

    /* Find optimal threshold by iterating from the edges inward */
    float best_threshold = cal->hist_max;
    double best_kl = 1e30;
    int quant_bins = 256; /* INT8 range */

    /* Try each bin as a clipping threshold */
    int start_bin = cal->num_bins / 2; /* At least half the range */
    for (int t = start_bin; t < cal->num_bins; t++) {
        /* Threshold at this bin edge */
        float threshold = cal->hist_min + ((float)t + 1.0f) * cal->bin_width;

        /* Compute KL-divergence between original and quantized distributions */
        double kl = 0.0;
        float quant_bin_width = (2.0f * threshold) / (float)quant_bins;
        if (quant_bin_width < INT8_SCALE_MIN) continue;

        for (int i = 0; i < cal->num_bins; i++) {
            if (cal->histogram[i] == 0) continue;
            float bin_center = cal->hist_min + ((float)i + 0.5f) * cal->bin_width;
            float clamped = fmaxf(-threshold, fminf(threshold, bin_center));

            /* Which quantized bin does this map to? */
            int qbin = (int)((clamped + threshold) / quant_bin_width);
            if (qbin < 0) qbin = 0;
            if (qbin >= quant_bins) qbin = quant_bins - 1;

            /* Dequantized center of that quantized bin */
            float dequant_center = ((float)qbin + 0.5f) * quant_bin_width - threshold;

            float diff = bin_center - dequant_center;
            double p = (double)cal->histogram[i] / (double)total_count;
            kl += p * (double)(diff * diff); /* Simplified MSE-based proxy for KL */
        }

        if (kl < best_kl) {
            best_kl = kl;
            best_threshold = threshold;
        }
    }

    /* Compute params from the optimal threshold */
    return nimcp_int8_compute_params_from_minmax(
        -best_threshold, best_threshold, true, params);
}

/* 18. nimcp_int8_calibrator_compute_percentile */
int nimcp_int8_calibrator_compute_percentile(
    nimcp_int8_calibrator_t* cal,
    float percentile,
    nimcp_int8_quant_params_t* params)
{
    if (!cal || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_calibrator_compute_percentile: required parameter is NULL");
        return -1;
    }

    if (!cal->histogram || cal->num_bins == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_calibrator_compute_percentile: no histogram data");
        return -1;
    }

    if (percentile <= 0.0f || percentile > 100.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_calibrator_compute_percentile: percentile out of range");
        return -1;
    }

    /* Count total samples in histogram */
    int total_count = 0;
    for (int i = 0; i < cal->num_bins; i++) {
        total_count += cal->histogram[i];
    }

    if (total_count == 0) {
        return nimcp_int8_compute_params_from_minmax(
            cal->hist_min, cal->hist_max, true, params);
    }

    float target_fraction = percentile / 100.0f;
    int target_count = (int)(target_fraction * (float)total_count);

    /* Find percentile from the right (max side) */
    int cumulative = 0;
    float max_val = cal->hist_max;
    for (int i = cal->num_bins - 1; i >= 0; i--) {
        cumulative += cal->histogram[i];
        if (cumulative >= (total_count - target_count)) {
            max_val = cal->hist_min + ((float)i + 1.0f) * cal->bin_width;
            break;
        }
    }

    /* Find percentile from the left (min side) */
    cumulative = 0;
    float min_val = cal->hist_min;
    for (int i = 0; i < cal->num_bins; i++) {
        cumulative += cal->histogram[i];
        if (cumulative >= (total_count - target_count)) {
            min_val = cal->hist_min + (float)i * cal->bin_width;
            break;
        }
    }

    bool symmetric = (cal->scheme == INT8_SCHEME_SYMMETRIC);
    return nimcp_int8_compute_params_from_minmax(min_val, max_val, symmetric, params);
}

/*=============================================================================
 * Quantization/Dequantization Operations (7 functions)
 *=============================================================================*/

/* 19. nimcp_int8_quantize */
int nimcp_int8_quantize(
    nimcp_gpu_context_t* ctx,
    const float* input,
    int8_t* output,
    size_t numel,
    const nimcp_int8_quant_params_t* params)
{
    (void)ctx;

    if (!input || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_quantize: required parameter is NULL");
        return -1;
    }

    float scale = params->scale;
    int32_t zp = params->zero_point;
    if (scale < INT8_SCALE_MIN) scale = INT8_SCALE_MIN;

    for (size_t i = 0; i < numel; i++) {
        output[i] = quantize_scalar(input[i], scale, zp);
    }

    return 0;
}

/* 20. nimcp_int8_quantize_tensor */
int nimcp_int8_quantize_tensor(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input,
    nimcp_int8_tensor_t* output)
{
    (void)ctx;

    if (!input || !input->data || !output || !output->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_quantize_tensor: required parameter is NULL");
        return -1;
    }

    size_t numel = input->numel < output->numel ? input->numel : output->numel;
    const float* fp32_data = (const float*)input->data;
    float scale = output->params.scale;
    int32_t zp = output->params.zero_point;
    if (scale < INT8_SCALE_MIN) scale = INT8_SCALE_MIN;

    for (size_t i = 0; i < numel; i++) {
        output->data[i] = quantize_scalar(fp32_data[i], scale, zp);
    }

    return 0;
}

/* 21. nimcp_int8_quantize_per_channel */
int nimcp_int8_quantize_per_channel(
    nimcp_gpu_context_t* ctx,
    const float* input,
    int8_t* output,
    int N,
    int C,
    int HW,
    const nimcp_int8_quant_params_t* params)
{
    (void)ctx;

    if (!input || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_quantize_per_channel: required parameter is NULL");
        return -1;
    }

    if (!params->channel_scales || params->num_channels < C) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_quantize_per_channel: missing per-channel scales");
        return -1;
    }

    /* Data layout: [N, C, HW] */
    for (int n = 0; n < N; n++) {
        for (int c = 0; c < C; c++) {
            float scale = params->channel_scales[c];
            int32_t zp = params->channel_zero_points ? params->channel_zero_points[c] : 0;
            if (scale < INT8_SCALE_MIN) scale = INT8_SCALE_MIN;

            size_t base_idx = (size_t)n * (size_t)C * (size_t)HW + (size_t)c * (size_t)HW;
            for (int hw = 0; hw < HW; hw++) {
                size_t idx = base_idx + (size_t)hw;
                output[idx] = quantize_scalar(input[idx], scale, zp);
            }
        }
    }

    return 0;
}

/* 22. nimcp_int8_dequantize */
int nimcp_int8_dequantize(
    nimcp_gpu_context_t* ctx,
    const int8_t* input,
    float* output,
    size_t numel,
    const nimcp_int8_quant_params_t* params)
{
    (void)ctx;

    if (!input || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_dequantize: required parameter is NULL");
        return -1;
    }

    float scale = params->scale;
    int32_t zp = params->zero_point;

    for (size_t i = 0; i < numel; i++) {
        output[i] = dequantize_scalar(input[i], scale, zp);
    }

    return 0;
}

/* 23. nimcp_int8_dequantize_tensor */
int nimcp_int8_dequantize_tensor(
    nimcp_gpu_context_t* ctx,
    const nimcp_int8_tensor_t* input,
    nimcp_gpu_tensor_t* output)
{
    (void)ctx;

    if (!input || !input->data || !output || !output->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_dequantize_tensor: required parameter is NULL");
        return -1;
    }

    size_t numel = input->numel < output->numel ? input->numel : output->numel;
    float* fp32_data = (float*)output->data;
    float scale = input->params.scale;
    int32_t zp = input->params.zero_point;

    for (size_t i = 0; i < numel; i++) {
        fp32_data[i] = dequantize_scalar(input->data[i], scale, zp);
    }

    return 0;
}

/* 24. nimcp_int8_dequantize_per_channel */
int nimcp_int8_dequantize_per_channel(
    nimcp_gpu_context_t* ctx,
    const int8_t* input,
    float* output,
    int N,
    int C,
    int HW,
    const nimcp_int8_quant_params_t* params)
{
    (void)ctx;

    if (!input || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_dequantize_per_channel: required parameter is NULL");
        return -1;
    }

    if (!params->channel_scales || params->num_channels < C) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_dequantize_per_channel: missing per-channel scales");
        return -1;
    }

    /* Data layout: [N, C, HW] */
    for (int n = 0; n < N; n++) {
        for (int c = 0; c < C; c++) {
            float scale = params->channel_scales[c];
            int32_t zp = params->channel_zero_points ? params->channel_zero_points[c] : 0;

            size_t base_idx = (size_t)n * (size_t)C * (size_t)HW + (size_t)c * (size_t)HW;
            for (int hw = 0; hw < HW; hw++) {
                size_t idx = base_idx + (size_t)hw;
                output[idx] = dequantize_scalar(input[idx], scale, zp);
            }
        }
    }

    return 0;
}

/* 25. nimcp_int8_fake_quantize */
int nimcp_int8_fake_quantize(
    nimcp_gpu_context_t* ctx,
    const float* input,
    float* output,
    size_t numel,
    const nimcp_int8_quant_params_t* params)
{
    (void)ctx;

    if (!input || !output || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_fake_quantize: required parameter is NULL");
        return -1;
    }

    float scale = params->scale;
    int32_t zp = params->zero_point;
    if (scale < INT8_SCALE_MIN) scale = INT8_SCALE_MIN;

    /* Fake quantize: quantize then immediately dequantize */
    for (size_t i = 0; i < numel; i++) {
        int8_t q = quantize_scalar(input[i], scale, zp);
        output[i] = dequantize_scalar(q, scale, zp);
    }

    return 0;
}

/*=============================================================================
 * INT8 Compute Kernels (8 functions)
 *=============================================================================*/

/* 26. nimcp_int8_gemm */
int nimcp_int8_gemm(
    nimcp_gpu_context_t* ctx,
    const int8_t* A,
    const int8_t* B,
    int32_t* C,
    int M,
    int N,
    int K,
    const nimcp_int8_quant_params_t* params_a,
    const nimcp_int8_quant_params_t* params_b)
{
    (void)ctx;
    (void)params_a;
    (void)params_b;

    if (!A || !B || !C) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_gemm: required parameter is NULL");
        return -1;
    }

    if (M <= 0 || N <= 0 || K <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_gemm: invalid dimensions");
        return -1;
    }

    /*
     * INT8 GEMM with INT32 accumulation: C[m][n] = sum_k(A[m][k] * B[k][n])
     * Performed entirely in integer arithmetic on CPU.
     */
    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            int32_t acc = 0;
            for (int k = 0; k < K; k++) {
                acc += (int32_t)A[m * K + k] * (int32_t)B[k * N + n];
            }
            C[m * N + n] = acc;
        }
    }

    return 0;
}

/* 27. nimcp_int8_gemm_fp32_output */
int nimcp_int8_gemm_fp32_output(
    nimcp_gpu_context_t* ctx,
    const int8_t* A,
    const int8_t* B,
    float* C,
    int M,
    int N,
    int K,
    const nimcp_int8_quant_params_t* params_a,
    const nimcp_int8_quant_params_t* params_b)
{
    (void)ctx;

    if (!A || !B || !C || !params_a || !params_b) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_gemm_fp32_output: required parameter is NULL");
        return -1;
    }

    if (M <= 0 || N <= 0 || K <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_gemm_fp32_output: invalid dimensions");
        return -1;
    }

    /*
     * Dequantize A and B, compute FP32 GEMM, output FP32.
     * C[m][n] = sum_k( (A[m][k] - zp_a) * scale_a * (B[k][n] - zp_b) * scale_b )
     *         = scale_a * scale_b * sum_k( (A[m][k] - zp_a) * (B[k][n] - zp_b) )
     */
    float combined_scale = params_a->scale * params_b->scale;
    int32_t zp_a = params_a->zero_point;
    int32_t zp_b = params_b->zero_point;

    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            int32_t acc = 0;
            for (int k = 0; k < K; k++) {
                int32_t a_val = (int32_t)A[m * K + k] - zp_a;
                int32_t b_val = (int32_t)B[k * N + n] - zp_b;
                acc += a_val * b_val;
            }
            C[m * N + n] = (float)acc * combined_scale;
        }
    }

    return 0;
}

/* 28. nimcp_int8_gemm_requant */
int nimcp_int8_gemm_requant(
    nimcp_gpu_context_t* ctx,
    const int8_t* A,
    const int8_t* B,
    int8_t* C,
    int M,
    int N,
    int K,
    const nimcp_int8_quant_params_t* params_a,
    const nimcp_int8_quant_params_t* params_b,
    const nimcp_int8_quant_params_t* params_c)
{
    (void)ctx;

    if (!A || !B || !C || !params_a || !params_b || !params_c) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_gemm_requant: required parameter is NULL");
        return -1;
    }

    if (M <= 0 || N <= 0 || K <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_gemm_requant: invalid dimensions");
        return -1;
    }

    /*
     * Compute FP32 result, then requantize to INT8 with params_c.
     * fp32_val = scale_a * scale_b * sum_k((A[m][k]-zp_a) * (B[k][n]-zp_b))
     * C[m][n] = clamp(round(fp32_val / scale_c) + zp_c, -128, 127)
     */
    float combined_scale = params_a->scale * params_b->scale;
    int32_t zp_a = params_a->zero_point;
    int32_t zp_b = params_b->zero_point;
    float scale_c = params_c->scale;
    int32_t zp_c = params_c->zero_point;
    if (scale_c < INT8_SCALE_MIN) scale_c = INT8_SCALE_MIN;

    for (int m = 0; m < M; m++) {
        for (int n = 0; n < N; n++) {
            int32_t acc = 0;
            for (int k = 0; k < K; k++) {
                int32_t a_val = (int32_t)A[m * K + k] - zp_a;
                int32_t b_val = (int32_t)B[k * N + n] - zp_b;
                acc += a_val * b_val;
            }
            float fp32_val = (float)acc * combined_scale;
            C[m * N + n] = quantize_scalar(fp32_val, scale_c, zp_c);
        }
    }

    return 0;
}

/* 29. nimcp_int8_conv2d */
int nimcp_int8_conv2d(
    nimcp_gpu_context_t* ctx,
    const int8_t* input,
    const int8_t* weight,
    int32_t* output,
    int N,
    int C_in,
    int H,
    int W,
    int C_out,
    int kH,
    int kW,
    int stride,
    int padding,
    const nimcp_int8_quant_params_t* params_in,
    const nimcp_int8_quant_params_t* params_w)
{
    (void)ctx;
    (void)params_in;
    (void)params_w;

    if (!input || !weight || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_conv2d: required parameter is NULL");
        return -1;
    }

    if (N <= 0 || C_in <= 0 || H <= 0 || W <= 0 ||
        C_out <= 0 || kH <= 0 || kW <= 0 || stride <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_conv2d: invalid dimensions");
        return -1;
    }

    /* Compute output spatial dimensions */
    int oH = (H + 2 * padding - kH) / stride + 1;
    int oW = (W + 2 * padding - kW) / stride + 1;

    if (oH <= 0 || oW <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_conv2d: output dimensions are non-positive");
        return -1;
    }

    /*
     * CPU im2col + INT8 GEMM approach:
     * For each batch element, for each output position, accumulate the
     * dot product of the input patch and the corresponding weight filter.
     *
     * Input layout:  [N, C_in, H, W]
     * Weight layout: [C_out, C_in, kH, kW]
     * Output layout: [N, C_out, oH, oW]
     *
     * Direct nested-loop implementation (no explicit im2col buffer needed).
     */
    for (int n = 0; n < N; n++) {
        for (int co = 0; co < C_out; co++) {
            for (int oh = 0; oh < oH; oh++) {
                for (int ow = 0; ow < oW; ow++) {
                    int32_t acc = 0;

                    for (int ci = 0; ci < C_in; ci++) {
                        for (int fh = 0; fh < kH; fh++) {
                            for (int fw = 0; fw < kW; fw++) {
                                int ih = oh * stride - padding + fh;
                                int iw = ow * stride - padding + fw;

                                int8_t in_val = 0;
                                if (ih >= 0 && ih < H && iw >= 0 && iw < W) {
                                    /* Input index: [n, ci, ih, iw] */
                                    size_t in_idx = (size_t)n * (size_t)C_in * (size_t)H * (size_t)W
                                                  + (size_t)ci * (size_t)H * (size_t)W
                                                  + (size_t)ih * (size_t)W
                                                  + (size_t)iw;
                                    in_val = input[in_idx];
                                }

                                /* Weight index: [co, ci, fh, fw] */
                                size_t w_idx = (size_t)co * (size_t)C_in * (size_t)kH * (size_t)kW
                                             + (size_t)ci * (size_t)kH * (size_t)kW
                                             + (size_t)fh * (size_t)kW
                                             + (size_t)fw;

                                acc += (int32_t)in_val * (int32_t)weight[w_idx];
                            }
                        }
                    }

                    /* Output index: [n, co, oh, ow] */
                    size_t out_idx = (size_t)n * (size_t)C_out * (size_t)oH * (size_t)oW
                                   + (size_t)co * (size_t)oH * (size_t)oW
                                   + (size_t)oh * (size_t)oW
                                   + (size_t)ow;
                    output[out_idx] = acc;
                }
            }
        }
    }

    return 0;
}

/* 30. nimcp_int8_add_requant */
int nimcp_int8_add_requant(
    nimcp_gpu_context_t* ctx,
    const int8_t* a,
    const int8_t* b,
    int8_t* c,
    size_t numel,
    const nimcp_int8_quant_params_t* params_a,
    const nimcp_int8_quant_params_t* params_b,
    const nimcp_int8_quant_params_t* params_c)
{
    (void)ctx;

    if (!a || !b || !c || !params_a || !params_b || !params_c) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_add_requant: required parameter is NULL");
        return -1;
    }

    /*
     * Dequantize both inputs to FP32, add, requantize to output INT8.
     * c[i] = quantize_c( dequantize_a(a[i]) + dequantize_b(b[i]) )
     */
    float scale_a = params_a->scale;
    int32_t zp_a = params_a->zero_point;
    float scale_b = params_b->scale;
    int32_t zp_b = params_b->zero_point;
    float scale_c = params_c->scale;
    int32_t zp_c = params_c->zero_point;
    if (scale_c < INT8_SCALE_MIN) scale_c = INT8_SCALE_MIN;

    for (size_t i = 0; i < numel; i++) {
        float fa = dequantize_scalar(a[i], scale_a, zp_a);
        float fb = dequantize_scalar(b[i], scale_b, zp_b);
        float sum = fa + fb;
        c[i] = quantize_scalar(sum, scale_c, zp_c);
    }

    return 0;
}

/* 31. nimcp_int8_relu */
int nimcp_int8_relu(
    nimcp_gpu_context_t* ctx,
    const int8_t* x,
    int8_t* y,
    size_t numel,
    int32_t zero_point)
{
    (void)ctx;

    if (!x || !y) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_relu: required parameter is NULL");
        return -1;
    }

    /*
     * INT8 ReLU: output = max(x, zero_point)
     * In quantized domain, zero_point represents real value 0.0.
     */
    int8_t zp = (int8_t)clamp_int8(zero_point);
    for (size_t i = 0; i < numel; i++) {
        y[i] = (x[i] > zp) ? x[i] : zp;
    }

    return 0;
}

/* 32. nimcp_int8_relu6 */
int nimcp_int8_relu6(
    nimcp_gpu_context_t* ctx,
    const int8_t* x,
    int8_t* y,
    size_t numel,
    const nimcp_int8_quant_params_t* params)
{
    (void)ctx;

    if (!x || !y || !params) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_relu6: required parameter is NULL");
        return -1;
    }

    /*
     * ReLU6: output = min(max(x, 0), 6)
     * In quantized domain: clamp between quantized(0) and quantized(6).
     */
    int8_t q_zero = quantize_scalar(0.0f, params->scale, params->zero_point);
    int8_t q_six  = quantize_scalar(6.0f, params->scale, params->zero_point);

    for (size_t i = 0; i < numel; i++) {
        int8_t val = x[i];
        if (val < q_zero) val = q_zero;
        if (val > q_six)  val = q_six;
        y[i] = val;
    }

    return 0;
}

/* 33. nimcp_int8_linear_relu */
int nimcp_int8_linear_relu(
    nimcp_gpu_context_t* ctx,
    const int8_t* input,
    const int8_t* weight,
    const void* bias,
    int8_t* output,
    int batch,
    int in_features,
    int out_features,
    const nimcp_int8_quant_params_t* params_in,
    const nimcp_int8_quant_params_t* params_w,
    const nimcp_int8_quant_params_t* params_out,
    bool bias_is_fp32)
{
    (void)ctx;

    if (!input || !weight || !output || !params_in || !params_w || !params_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_linear_relu: required parameter is NULL");
        return -1;
    }

    if (batch <= 0 || in_features <= 0 || out_features <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_linear_relu: invalid dimensions");
        return -1;
    }

    /*
     * Fused linear + ReLU:
     * y = ReLU(x @ W^T + b)
     *
     * 1. Dequantize input and weight to FP32
     * 2. Compute matmul + bias
     * 3. Apply ReLU
     * 4. Requantize to output INT8
     *
     * Input:  [batch, in_features]
     * Weight: [out_features, in_features]
     * Output: [batch, out_features]
     */
    float scale_in = params_in->scale;
    int32_t zp_in = params_in->zero_point;
    float scale_w = params_w->scale;
    int32_t zp_w = params_w->zero_point;
    float scale_out = params_out->scale;
    int32_t zp_out = params_out->zero_point;
    if (scale_out < INT8_SCALE_MIN) scale_out = INT8_SCALE_MIN;

    float combined_scale = scale_in * scale_w;

    const float* bias_fp32 = bias_is_fp32 ? (const float*)bias : NULL;
    const int32_t* bias_int32 = bias_is_fp32 ? NULL : (const int32_t*)bias;

    for (int b_idx = 0; b_idx < batch; b_idx++) {
        for (int o = 0; o < out_features; o++) {
            /* Compute dot product: input[b_idx] . weight[o] */
            int32_t acc = 0;
            for (int i = 0; i < in_features; i++) {
                int32_t a_val = (int32_t)input[b_idx * in_features + i] - zp_in;
                int32_t w_val = (int32_t)weight[o * in_features + i] - zp_w;
                acc += a_val * w_val;
            }

            float fp32_val = (float)acc * combined_scale;

            /* Add bias */
            if (bias) {
                if (bias_fp32) {
                    fp32_val += bias_fp32[o];
                } else if (bias_int32) {
                    /* INT32 bias: use combined input*weight scale for dequantization */
                    fp32_val += (float)bias_int32[o] * combined_scale;
                }
            }

            /* ReLU */
            if (fp32_val < 0.0f) fp32_val = 0.0f;

            /* Requantize to output */
            output[b_idx * out_features + o] = quantize_scalar(fp32_val, scale_out, zp_out);
        }
    }

    return 0;
}

/*=============================================================================
 * Model Quantization API (6 functions)
 *=============================================================================*/

/* 34. nimcp_int8_model_create */
nimcp_int8_model_t* nimcp_int8_model_create(
    nimcp_gpu_context_t* ctx,
    int num_layers,
    const char* model_name)
{
    (void)ctx;

    if (num_layers <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_model_create: num_layers <= 0");
        return NULL;
    }

    nimcp_int8_model_t* model = nimcp_calloc(1, sizeof(nimcp_int8_model_t));
    if (!model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_model_create: allocation failed");
        return NULL;
    }

    model->ctx = ctx;
    model->num_layers = num_layers;
    model->mode = INT8_QUANT_MODE_STATIC;
    model->calibrated = false;
    model->inference_count = 0;
    model->avg_latency_ms = 0.0f;
    model->workspace = NULL;
    model->workspace_size = 0;

    if (model_name) {
        strncpy(model->model_name, model_name, sizeof(model->model_name) - 1);
        model->model_name[sizeof(model->model_name) - 1] = '\0';
    }

    model->layers = nimcp_calloc((size_t)num_layers, sizeof(nimcp_int8_layer_t));
    if (!model->layers) {
        nimcp_free(model);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_model_create: layers allocation failed");
        return NULL;
    }

    /* Initialize layer quant params */
    for (int i = 0; i < num_layers; i++) {
        nimcp_int8_params_init(&model->layers[i].input_params);
        nimcp_int8_params_init(&model->layers[i].output_params);
    }

    return model;
}

/* 35. nimcp_int8_model_destroy */
void nimcp_int8_model_destroy(nimcp_int8_model_t* model) {
    if (!model) return;

    if (model->layers) {
        for (int i = 0; i < model->num_layers; i++) {
            nimcp_int8_tensor_destroy(model->layers[i].weight);
            nimcp_int8_tensor_destroy(model->layers[i].bias);
        }
        nimcp_free(model->layers);
    }

    nimcp_free(model->workspace);
    nimcp_free(model);
}

/* 36. nimcp_int8_model_add_layer */
int nimcp_int8_model_add_layer(
    nimcp_int8_model_t* model,
    int layer_idx,
    const char* layer_name,
    const nimcp_gpu_tensor_t* weight_fp32,
    const nimcp_gpu_tensor_t* bias_fp32,
    const nimcp_int8_quant_params_t* weight_params)
{
    if (!model || !weight_fp32 || !weight_fp32->data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_model_add_layer: required parameter is NULL");
        return -1;
    }

    if (layer_idx < 0 || layer_idx >= model->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_model_add_layer: layer_idx out of range");
        return -1;
    }

    nimcp_int8_layer_t* layer = &model->layers[layer_idx];

    /* Set layer name */
    if (layer_name) {
        strncpy(layer->name, layer_name, sizeof(layer->name) - 1);
        layer->name[sizeof(layer->name) - 1] = '\0';
    }

    /* Compute weight quantization params if not provided */
    nimcp_int8_quant_params_t w_params;
    if (weight_params) {
        w_params = *weight_params;
    } else {
        nimcp_int8_params_init(&w_params);
        /* Auto-compute from weight data */
        const float* w_data = (const float*)weight_fp32->data;
        float wmin = FLT_MAX, wmax = -FLT_MAX;
        for (size_t i = 0; i < weight_fp32->numel; i++) {
            if (w_data[i] < wmin) wmin = w_data[i];
            if (w_data[i] > wmax) wmax = w_data[i];
        }
        nimcp_int8_compute_params_from_minmax(wmin, wmax, true, &w_params);
    }

    /* Quantize weight FP32 -> INT8 */
    layer->weight = nimcp_int8_tensor_from_fp32(weight_fp32, &w_params);
    if (!layer->weight) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_model_add_layer: weight quantization failed");
        return -1;
    }

    /* Handle bias (keep as FP32 for accuracy or quantize) */
    if (bias_fp32 && bias_fp32->data) {
        layer->bias_is_fp32 = true;

        /* Store bias as an INT8 tensor struct but with FP32 semantics */
        layer->bias = nimcp_calloc(1, sizeof(nimcp_int8_tensor_t));
        if (layer->bias) {
            layer->bias->rank = (size_t)bias_fp32->ndim;
            layer->bias->numel = bias_fp32->numel;
            layer->bias->dims = nimcp_malloc((size_t)bias_fp32->ndim * sizeof(size_t));
            if (layer->bias->dims) {
                memcpy(layer->bias->dims, bias_fp32->dims,
                       (size_t)bias_fp32->ndim * sizeof(size_t));
            }
            /* Allocate data as int8_t buffer but sized for floats */
            layer->bias->data = (int8_t*)nimcp_malloc(bias_fp32->numel * sizeof(float));
            if (layer->bias->data) {
                memcpy(layer->bias->data, bias_fp32->data,
                       bias_fp32->numel * sizeof(float));
            }
            layer->bias->owns_data = true;
            nimcp_int8_params_init(&layer->bias->params);
        }
    }

    return 0;
}

/* 37. nimcp_int8_model_calibrate */
int nimcp_int8_model_calibrate(
    nimcp_int8_model_t* model,
    void (*forward_fn)(void* ctx, nimcp_gpu_tensor_t* input, nimcp_gpu_tensor_t* output),
    void* forward_ctx,
    nimcp_gpu_tensor_t** calibration_data,
    int num_samples)
{
    if (!model || !forward_fn || !calibration_data) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_model_calibrate: required parameter is NULL");
        return -1;
    }

    if (num_samples <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_model_calibrate: num_samples <= 0");
        return -1;
    }

    /*
     * CPU fallback calibration:
     * Run forward passes on calibration data to collect activation statistics.
     * Since we don't have layer hooks in this simple stub, we just run the
     * forward function and mark the model as calibrated.
     *
     * A full implementation would instrument each layer to collect statistics
     * and compute optimal per-layer quantization parameters.
     */
    for (int s = 0; s < num_samples; s++) {
        if (!calibration_data[s]) continue;

        /* Allocate output tensor matching input dims for forward pass */
        nimcp_gpu_tensor_t* output = nimcp_gpu_tensor_create(
            model->ctx, calibration_data[s]->dims, calibration_data[s]->ndim,
            NIMCP_GPU_PRECISION_FP32);
        if (!output) continue;

        forward_fn(forward_ctx, calibration_data[s], output);
        nimcp_gpu_tensor_destroy(output);
    }

    model->calibrated = true;
    return 0;
}

/* 38. nimcp_int8_model_set_act_params */
int nimcp_int8_model_set_act_params(
    nimcp_int8_model_t* model,
    int layer_idx,
    const nimcp_int8_quant_params_t* input_params,
    const nimcp_int8_quant_params_t* output_params)
{
    if (!model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_model_set_act_params: model is NULL");
        return -1;
    }

    if (layer_idx < 0 || layer_idx >= model->num_layers) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_model_set_act_params: layer_idx out of range");
        return -1;
    }

    nimcp_int8_layer_t* layer = &model->layers[layer_idx];

    if (input_params) {
        layer->input_params = *input_params;
        /* Clear per-channel/group pointers (shallow copy) */
        layer->input_params.channel_scales = NULL;
        layer->input_params.channel_zero_points = NULL;
        layer->input_params.group_scales = NULL;
        layer->input_params.group_zero_points = NULL;
    }

    if (output_params) {
        layer->output_params = *output_params;
        layer->output_params.channel_scales = NULL;
        layer->output_params.channel_zero_points = NULL;
        layer->output_params.group_scales = NULL;
        layer->output_params.group_zero_points = NULL;
    }

    return 0;
}

/* 39. nimcp_int8_model_allocate_workspace */
int nimcp_int8_model_allocate_workspace(
    nimcp_int8_model_t* model,
    int max_batch_size)
{
    if (!model) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_model_allocate_workspace: model is NULL");
        return -1;
    }

    if (max_batch_size <= 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "nimcp_int8_model_allocate_workspace: max_batch_size <= 0");
        return -1;
    }

    /* Free existing workspace */
    nimcp_free(model->workspace);

    /*
     * Allocate workspace for intermediate computation buffers.
     * Size heuristic: enough for the largest intermediate activation.
     * For CPU fallback, we allocate a reasonable workspace.
     */
    size_t max_layer_size = 0;
    for (int i = 0; i < model->num_layers; i++) {
        if (model->layers[i].weight && model->layers[i].weight->numel > max_layer_size) {
            max_layer_size = model->layers[i].weight->numel;
        }
    }

    /* Workspace: max_batch_size * max_layer_size * sizeof(float) for FP32 intermediates */
    model->workspace_size = (size_t)max_batch_size * max_layer_size * sizeof(float);
    if (model->workspace_size == 0) {
        /* Minimum workspace */
        model->workspace_size = (size_t)max_batch_size * 1024 * sizeof(float);
    }

    model->workspace = nimcp_calloc(1, model->workspace_size);
    if (!model->workspace) {
        model->workspace_size = 0;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "nimcp_int8_model_allocate_workspace: workspace allocation failed");
        return -1;
    }

    return 0;
}

/*=============================================================================
 * Utility Functions (9 functions)
 *=============================================================================*/

/* 40. nimcp_int8_scheme_name */
const char* nimcp_int8_scheme_name(nimcp_int8_scheme_t scheme) {
    switch (scheme) {
        case INT8_SCHEME_SYMMETRIC:  return "symmetric";
        case INT8_SCHEME_ASYMMETRIC: return "asymmetric";
        default:                     return "unknown";
    }
}

/* 41. nimcp_int8_calib_method_name */
const char* nimcp_int8_calib_method_name(nimcp_int8_calib_method_t method) {
    switch (method) {
        case INT8_CALIB_MINMAX:     return "minmax";
        case INT8_CALIB_HISTOGRAM:  return "histogram";
        case INT8_CALIB_ENTROPY:    return "entropy";
        case INT8_CALIB_PERCENTILE: return "percentile";
        case INT8_CALIB_MSE:        return "mse";
        default:                    return "unknown";
    }
}

/* 42. nimcp_int8_mode_name */
const char* nimcp_int8_mode_name(nimcp_int8_quant_mode_t mode) {
    switch (mode) {
        case INT8_QUANT_MODE_DYNAMIC: return "dynamic";
        case INT8_QUANT_MODE_STATIC:  return "static";
        case INT8_QUANT_MODE_QAT:     return "qat";
        default:                      return "unknown";
    }
}

/* 43. nimcp_int8_memory_savings */
size_t nimcp_int8_memory_savings(size_t fp32_size) {
    /* INT8 uses 1 byte per element vs FP32's 4 bytes, plus small overhead for params */
    return (fp32_size + 3) / 4;  /* ceil(fp32_size / 4) */
}

/* 44. nimcp_int8_compute_mse */
float nimcp_int8_compute_mse(
    nimcp_gpu_context_t* ctx,
    const float* original,
    const int8_t* int8_data,
    size_t numel,
    const nimcp_int8_quant_params_t* params)
{
    (void)ctx;

    if (!original || !int8_data || !params || numel == 0) {
        return 0.0f;
    }

    double mse = 0.0;
    float scale = params->scale;
    int32_t zp = params->zero_point;

    for (size_t i = 0; i < numel; i++) {
        float dequant = dequantize_scalar(int8_data[i], scale, zp);
        double diff = (double)(original[i] - dequant);
        mse += diff * diff;
    }

    return (float)(mse / (double)numel);
}

/* 45. nimcp_int8_compute_sqnr */
float nimcp_int8_compute_sqnr(
    nimcp_gpu_context_t* ctx,
    const float* original,
    const int8_t* int8_data,
    size_t numel,
    const nimcp_int8_quant_params_t* params)
{
    (void)ctx;

    if (!original || !int8_data || !params || numel == 0) {
        return 0.0f;
    }

    double signal_power = 0.0;
    double noise_power = 0.0;
    float scale = params->scale;
    int32_t zp = params->zero_point;

    for (size_t i = 0; i < numel; i++) {
        float dequant = dequantize_scalar(int8_data[i], scale, zp);
        double sig = (double)original[i];
        double noise = sig - (double)dequant;
        signal_power += sig * sig;
        noise_power += noise * noise;
    }

    if (noise_power < 1e-30) {
        return 200.0f; /* Essentially perfect quantization */
    }

    /* SQNR in dB = 10 * log10(signal_power / noise_power) */
    return (float)(10.0 * log10(signal_power / noise_power));
}

/* 46. nimcp_int8_print_params */
void nimcp_int8_print_params(
    const nimcp_int8_quant_params_t* params,
    const char* name)
{
    if (!params) {
        printf("[INT8] (null params)\n");
        return;
    }

    printf("[INT8] %s: scale=%.6g, zero_point=%d, range=[%.6g, %.6g], %s",
           name ? name : "unnamed",
           (double)params->scale,
           (int)params->zero_point,
           (double)params->min_val,
           (double)params->max_val,
           params->symmetric ? "symmetric" : "asymmetric");

    switch (params->granularity) {
        case INT8_GRANULARITY_TENSOR:
            printf(", per-tensor");
            break;
        case INT8_GRANULARITY_CHANNEL:
            printf(", per-channel (ch=%d)", params->num_channels);
            break;
        case INT8_GRANULARITY_GROUP:
            printf(", per-group (groups=%d, size=%d)", params->num_groups, params->group_size);
            break;
        default:
            break;
    }

    printf("\n");
}

/* 47. nimcp_int8_tensor_cores_available */
bool nimcp_int8_tensor_cores_available(nimcp_gpu_context_t* ctx) {
    (void)ctx;
    /* CPU fallback: no tensor cores available */
    return false;
}

/* 48. nimcp_int8_get_recommended_settings */
int nimcp_int8_get_recommended_settings(
    nimcp_gpu_context_t* ctx,
    nimcp_int8_scheme_t* scheme,
    nimcp_int8_granularity_t* granularity)
{
    (void)ctx;

    if (!scheme || !granularity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "nimcp_int8_get_recommended_settings: required parameter is NULL");
        return -1;
    }

    /*
     * CPU fallback: recommend symmetric per-channel quantization.
     * Symmetric avoids zero_point overhead in the accumulator.
     * Per-channel provides better accuracy for weights.
     */
    *scheme = INT8_SCHEME_SYMMETRIC;
    *granularity = INT8_GRANULARITY_CHANNEL;

    return 0;
}
