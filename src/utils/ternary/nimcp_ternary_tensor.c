#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_ternary_tensor.c - Ternary-Tensor Integration Implementation
//=============================================================================
/**
 * @file nimcp_ternary_tensor.c
 * @brief Implementation of ternary-tensor conversion and operations
 *
 * WHAT: Bridges ternary logic module with NIMCP tensor library
 * WHY:  Enable ternary compression and operations on tensor data
 * HOW:  Type-safe conversions with threshold quantization
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#include "utils/ternary/nimcp_ternary_tensor.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(ternary_tensor)

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Get element from tensor as float
 */
static float tensor_get_float(const nimcp_tensor_t* tensor, size_t idx) {
    if (!tensor) return 0.0f;

    /* Get flat index value - handle different dtypes */
    double val = nimcp_tensor_get_flat(tensor, idx);
    return (float)val;
}

/**
 * @brief Get total number of elements in tensor
 */
static size_t tensor_numel(const nimcp_tensor_t* tensor) {
    if (!tensor) return 0;

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(tensor);
    if (!shape) return 0;

    return shape->numel;
}

//=============================================================================
// Tensor to Ternary Conversion
//=============================================================================

trit_vector_t* trit_vector_from_tensor(
    const nimcp_tensor_t* tensor,
    float threshold,
    ternary_pack_mode_t pack_mode
) {
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }

    size_t numel = tensor_numel(tensor);
    if (numel == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "trit_vector_from_tensor: numel is zero");
        return NULL;
    }

    trit_vector_t* vec = trit_vector_create(numel, pack_mode);
    if (!vec) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "vec is NULL");

        return NULL;

    }

    for (size_t i = 0; i < numel; i++) {
        float val = tensor_get_float(tensor, i);
        trit_t t = trit_from_float_threshold(val, threshold);
        trit_vector_set(vec, i, t);
    }

    return vec;
}

trit_matrix_t* trit_matrix_from_tensor(
    const nimcp_tensor_t* tensor,
    float threshold,
    ternary_pack_mode_t pack_mode
) {
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(tensor);
    if (!shape || shape->rank != 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trit_matrix_from_tensor: shape is NULL");
        return NULL;
    }

    size_t rows = shape->dims[0];
    size_t cols = shape->dims[1];

    trit_matrix_t* mat = trit_matrix_create(rows, cols, pack_mode);
    if (!mat) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mat is NULL");

        return NULL;

    }

    for (size_t row = 0; row < rows; row++) {
        for (size_t col = 0; col < cols; col++) {
            size_t idx = row * cols + col;
            float val = tensor_get_float(tensor, idx);
            trit_t t = trit_from_float_threshold(val, threshold);
            trit_matrix_set(mat, row, col, t);
        }
    }

    return mat;
}

trit_vector_t* trit_vector_from_tensor_flat(
    const nimcp_tensor_t* tensor,
    float threshold,
    ternary_pack_mode_t pack_mode
) {
    /* Same as trit_vector_from_tensor - flattens any rank */
    return trit_vector_from_tensor(tensor, threshold, pack_mode);
}

//=============================================================================
// Ternary to Tensor Conversion
//=============================================================================

nimcp_tensor_t* trit_vector_to_tensor(
    const trit_vector_t* vec,
    float scale,
    nimcp_dtype_t dtype
) {
    if (!vec || vec->magic != TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trit_vector_to_tensor: vec is NULL");
        return NULL;
    }

    uint32_t dims[1] = { (uint32_t)vec->length };
    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 1, dtype);
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }

    for (size_t i = 0; i < vec->length; i++) {
        double val = (double)trit_vector_get(vec, i) * scale;
        nimcp_tensor_set_flat(tensor, i, val);
    }

    return tensor;
}

nimcp_tensor_t* trit_matrix_to_tensor(
    const trit_matrix_t* mat,
    float scale,
    nimcp_dtype_t dtype
) {
    if (!mat || mat->magic != TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trit_matrix_to_tensor: mat is NULL");
        return NULL;
    }

    uint32_t dims[2] = { (uint32_t)mat->rows, (uint32_t)mat->cols };
    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, dtype);
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }

    for (size_t row = 0; row < mat->rows; row++) {
        for (size_t col = 0; col < mat->cols; col++) {
            size_t idx = row * mat->cols + col;
            double val = (double)trit_matrix_get(mat, row, col) * scale;
            nimcp_tensor_set_flat(tensor, idx, val);
        }
    }

    return tensor;
}

nimcp_tensor_t* trit_vector_to_tensor_shaped(
    const trit_vector_t* vec,
    const uint32_t* dims,
    uint32_t rank,
    float scale,
    nimcp_dtype_t dtype
) {
    if (!vec || vec->magic != TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trit_vector_to_tensor_shaped: vec is NULL");
        return NULL;
    }
    if (!dims || rank == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "trit_vector_to_tensor_shaped: dims is NULL");
        return NULL;
    }

    /* Calculate expected numel */
    size_t numel = 1;
    for (uint32_t i = 0; i < rank; i++) {
        numel *= dims[i];
    }

    if (numel != vec->length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "trit_vector_to_tensor_shaped: validation failed");
        return NULL;
    }

    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, rank, dtype);
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }

    for (size_t i = 0; i < vec->length; i++) {
        double val = (double)trit_vector_get(vec, i) * scale;
        nimcp_tensor_set_flat(tensor, i, val);
    }

    return tensor;
}

//=============================================================================
// Adaptive Quantization
//=============================================================================

ternary_error_t trit_analyze_tensor(
    const nimcp_tensor_t* tensor,
    float trial_threshold,
    trit_quantization_stats_t* stats
) {
    if (!tensor || !stats) return TERNARY_ERR_NULL;

    size_t numel = tensor_numel(tensor);
    if (numel == 0) return TERNARY_ERR_SHAPE;

    /* Compute mean */
    double sum = 0.0;
    for (size_t i = 0; i < numel; i++) {
        sum += tensor_get_float(tensor, i);
    }
    double mean = sum / numel;

    /* Compute std */
    double sum_sq = 0.0;
    for (size_t i = 0; i < numel; i++) {
        double diff = tensor_get_float(tensor, i) - mean;
        sum_sq += diff * diff;
    }
    double std = sqrt(sum_sq / numel);

    /* Determine threshold */
    float threshold = trial_threshold;
    if (threshold <= 0.0f) {
        threshold = (float)(0.5 * std);  /* Default: half std */
    }

    /* Count quantization distribution */
    size_t n_pos = 0, n_unk = 0, n_neg = 0;
    for (size_t i = 0; i < numel; i++) {
        float val = tensor_get_float(tensor, i);
        if (val >= threshold) n_pos++;
        else if (val <= -threshold) n_neg++;
        else n_unk++;
    }

    /* Fill stats */
    stats->mean = (float)mean;
    stats->std = (float)std;
    stats->suggested_threshold = (float)(0.5 * std);
    stats->n_positive = n_pos;
    stats->n_unknown = n_unk;
    stats->n_negative = n_neg;
    stats->sparsity = (float)n_unk / (float)numel;

    /* Compute compression ratio (F32 to base-243 packed) */
    size_t original_bytes = numel * sizeof(float);
    size_t packed_bytes = trit_packed_bytes(numel, TERNARY_PACK_BASE243);
    stats->compression_ratio = (float)original_bytes / (float)packed_bytes;

    return TERNARY_OK;
}

trit_vector_t* trit_quantize_adaptive(
    const nimcp_tensor_t* tensor,
    float target_sparsity,
    ternary_pack_mode_t pack_mode,
    trit_quantization_stats_t* actual_stats
) {
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }

    size_t numel = tensor_numel(tensor);
    if (numel == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "trit_quantize_adaptive: numel is zero");
        return NULL;
    }

    /* Compute std for threshold estimation */
    double sum = 0.0, sum_sq = 0.0;
    for (size_t i = 0; i < numel; i++) {
        float val = tensor_get_float(tensor, i);
        sum += val;
        sum_sq += val * val;
    }
    double mean = sum / numel;
    double var = (sum_sq / numel) - (mean * mean);
    double std = sqrt(var > 0 ? var : 0);

    /* Binary search for threshold that achieves target sparsity */
    float lo = 0.0f;
    float hi = (float)(3.0 * std);
    float threshold = (float)(0.5 * std);

    for (int iter = 0; iter < 20; iter++) {
        size_t n_unk = 0;
        for (size_t i = 0; i < numel; i++) {
            float val = tensor_get_float(tensor, i);
            if (val < threshold && val > -threshold) n_unk++;
        }
        float sparsity = (float)n_unk / (float)numel;

        if (fabsf(sparsity - target_sparsity) < 0.01f) break;

        if (sparsity < target_sparsity) {
            lo = threshold;
        } else {
            hi = threshold;
        }
        threshold = (lo + hi) / 2.0f;
    }

    /* Create quantized vector */
    trit_vector_t* vec = trit_vector_from_tensor(tensor, threshold, pack_mode);

    /* Compute actual stats if requested */
    if (vec && actual_stats) {
        actual_stats->mean = (float)mean;
        actual_stats->std = (float)std;
        actual_stats->suggested_threshold = threshold;
        trit_vector_count(vec, &actual_stats->n_positive,
                          &actual_stats->n_unknown, &actual_stats->n_negative);
        actual_stats->sparsity = (float)actual_stats->n_unknown / (float)numel;

        size_t original_bytes = numel * sizeof(float);
        size_t packed_bytes = trit_packed_bytes(numel, pack_mode);
        actual_stats->compression_ratio = (float)original_bytes / (float)packed_bytes;
    }

    return vec;
}

//=============================================================================
// Ternary Tensor Operations
//=============================================================================

ternary_error_t trit_mask_tensor(
    nimcp_tensor_t* tensor,
    const trit_vector_t* mask
) {
    if (!tensor || !mask || mask->magic != TERNARY_MAGIC) return TERNARY_ERR_NULL;

    size_t numel = tensor_numel(tensor);
    if (numel != mask->length) return TERNARY_ERR_SHAPE;

    for (size_t i = 0; i < numel; i++) {
        trit_t m = trit_vector_get(mask, i);
        double val = nimcp_tensor_get_flat(tensor, i);
        nimcp_tensor_set_flat(tensor, i, val * (double)m);
    }

    return TERNARY_OK;
}

nimcp_tensor_t* trit_gate_tensor(
    const nimcp_tensor_t* tensor,
    const trit_vector_t* gate
) {
    if (!tensor || !gate || gate->magic != TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trit_gate_tensor: required parameter is NULL (tensor, gate)");
        return NULL;
    }

    size_t numel = tensor_numel(tensor);
    if (numel != gate->length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "trit_gate_tensor: validation failed");
        return NULL;
    }

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(tensor);
    nimcp_dtype_t dtype = nimcp_tensor_dtype(tensor);

    nimcp_tensor_t* result = nimcp_tensor_create(shape->dims, shape->rank, dtype);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    for (size_t i = 0; i < numel; i++) {
        trit_t g = trit_vector_get(gate, i);
        double val = nimcp_tensor_get_flat(tensor, i);
        nimcp_tensor_set_flat(result, i, val * (double)g);
    }

    return result;
}

nimcp_tensor_t* trit_matmul_tensor(
    const trit_matrix_t* weights,
    const nimcp_tensor_t* input,
    float weight_scale
) {
    if (!weights || weights->magic != TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trit_matmul_tensor: weights is NULL");
        return NULL;
    }
    if (!input) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "input is NULL");

        return NULL;

    }

    size_t input_numel = tensor_numel(input);
    if (input_numel != weights->cols) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "trit_matmul_tensor: validation failed");
        return NULL;
    }

    /* Output is 1D tensor with weights->rows elements */
    uint32_t dims[1] = { (uint32_t)weights->rows };
    nimcp_tensor_t* output = nimcp_tensor_zeros(dims, 1, NIMCP_DTYPE_F32);
    if (!output) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "output is NULL");

        return NULL;

    }

    for (size_t row = 0; row < weights->rows; row++) {
        double sum = 0.0;
        for (size_t col = 0; col < weights->cols; col++) {
            trit_t w = trit_matrix_get(weights, row, col);
            double in_val = nimcp_tensor_get_flat(input, col);
            sum += (double)w * weight_scale * in_val;
        }
        nimcp_tensor_set_flat(output, row, sum);
    }

    return output;
}

//=============================================================================
// Gradient-Aware Quantization
//=============================================================================

nimcp_tensor_t* trit_quantize_ste(
    const nimcp_tensor_t* tensor,
    float threshold
) {
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(tensor);
    size_t numel = shape->numel;

    nimcp_tensor_t* result = nimcp_tensor_create(shape->dims, shape->rank, NIMCP_DTYPE_F32);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    for (size_t i = 0; i < numel; i++) {
        float val = tensor_get_float(tensor, i);
        trit_t t = trit_from_float_threshold(val, threshold);
        nimcp_tensor_set_flat(result, i, (double)t);
    }

    return result;
}

nimcp_tensor_t* trit_quantize_soft(
    const nimcp_tensor_t* tensor,
    float temperature
) {
    if (!tensor) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tensor is NULL");

        return NULL;

    }
    if (temperature <= 0.0f) temperature = 1.0f;

    const nimcp_tensor_shape_t* shape = nimcp_tensor_shape(tensor);
    size_t numel = shape->numel;

    nimcp_tensor_t* result = nimcp_tensor_create(shape->dims, shape->rank, NIMCP_DTYPE_F32);
    if (!result) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "result is NULL");

        return NULL;

    }

    for (size_t i = 0; i < numel; i++) {
        float val = tensor_get_float(tensor, i);
        /* Soft ternary: approximate sign function with tanh */
        float soft = tanhf(val / temperature);
        nimcp_tensor_set_flat(result, i, (double)soft);
    }

    return result;
}

//=============================================================================
// Statistics Helpers
//=============================================================================

float trit_tensor_sparsity(
    const nimcp_tensor_t* tensor,
    float threshold
) {
    if (!tensor) return 0.0f;

    size_t numel = tensor_numel(tensor);
    if (numel == 0) return 0.0f;

    size_t n_zero = 0;
    for (size_t i = 0; i < numel; i++) {
        float val = tensor_get_float(tensor, i);
        if (val < threshold && val > -threshold) n_zero++;
    }

    return (float)n_zero / (float)numel;
}

float trit_quantization_error(
    const nimcp_tensor_t* tensor,
    float threshold,
    float scale
) {
    if (!tensor) return 0.0f;

    size_t numel = tensor_numel(tensor);
    if (numel == 0) return 0.0f;

    double sum_sq_error = 0.0;
    for (size_t i = 0; i < numel; i++) {
        float original = tensor_get_float(tensor, i);
        trit_t quantized = trit_from_float_threshold(original, threshold);
        float dequantized = (float)quantized * scale;
        float error = original - dequantized;
        sum_sq_error += error * error;
    }

    return sqrtf((float)(sum_sq_error / numel));
}
