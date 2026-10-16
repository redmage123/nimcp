/**
 * @file nimcp_lnn_ternary.c
 * @brief Ternary Weight Implementation for Liquid Neural Networks
 * @version 1.0.0
 * @date 2025-12-31
 *
 * WHAT: Ternary weight matrices and sparse operations for LNN
 * WHY:  20x memory savings with minimal accuracy loss
 * HOW:  Threshold quantization with CSR sparse matmul
 *
 * @author NIMCP Development Team
 */

#include "lnn/nimcp_lnn_ternary.h"
#include "lnn/nimcp_lnn_types.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(lnn_ternary)

/*=============================================================================
 * Internal Helper Functions
 *===========================================================================*/

/**
 * @brief Compute scale factor from float weights
 *
 * WHAT: Calculate maximum absolute weight value
 * WHY:  Needed for dequantization scaling
 * HOW:  Linear scan for max |w|
 */
static float compute_scale_factor(const nimcp_tensor_t* weights) {
    if (!weights) return 1.0f;

    const float* data = nimcp_tensor_data_const(weights);
    size_t numel = nimcp_tensor_numel(weights);
    float max_abs = 0.0f;

    for (size_t i = 0; i < numel; i++) {
        float abs_val = fabsf(data[i]);
        if (abs_val > max_abs) {
            max_abs = abs_val;
        }
    }

    return (max_abs > 0.0f) ? max_abs : 1.0f;
}

/**
 * @brief Count non-zero ternary elements
 *
 * WHAT: Count elements that will be non-zero after quantization
 * WHY:  Pre-allocate sparse storage
 * HOW:  Count |w| >= threshold
 */
static uint32_t count_nonzeros(
    const nimcp_tensor_t* weights,
    float threshold
) {
    if (!weights) return 0;

    const float* data = nimcp_tensor_data_const(weights);
    size_t numel = nimcp_tensor_numel(weights);
    uint32_t nnz = 0;

    for (size_t i = 0; i < numel; i++) {
        if (fabsf(data[i]) >= threshold) {
            nnz++;
        }
    }

    return nnz;
}

/**
 * @brief Binary search for column in sparse row
 *
 * WHAT: Find column index within row's non-zeros
 * WHY:  Efficient element access in CSR format
 * HOW:  Binary search in sorted col_idx segment
 */
static int32_t sparse_find_col(
    const uint32_t* col_idx,
    uint32_t start,
    uint32_t end,
    uint32_t col
) {
    while (start < end) {
        uint32_t mid = start + (end - start) / 2;
        if (col_idx[mid] == col) {
            return (int32_t)mid;
        } else if (col_idx[mid] < col) {
            start = mid + 1;
        } else {
            end = mid;
        }
    }
    return -1;  /* Not found - column not in sparse row */
}

/*=============================================================================
 * Lifecycle Functions
 *===========================================================================*/

lnn_ternary_matrix_t* lnn_ternary_matrix_create(
    uint32_t rows,
    uint32_t cols,
    ternary_pack_mode_t pack_mode,
    bool use_sparse
) {
    /* Guard: validate dimensions */
    if (rows == 0 || cols == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid dimensions: rows=%u, cols=%u", rows, cols);
        return NULL;
    }

    /* Allocate structure */
    lnn_ternary_matrix_t* mat = nimcp_malloc(sizeof(lnn_ternary_matrix_t));
    if (!mat) {
        NIMCP_THROW_MEMORY(NIMCP_ERROR_NO_MEMORY, sizeof(lnn_ternary_matrix_t),
                          "Failed to allocate ternary matrix");
        return NULL;
    }

    /* Initialize structure */
    memset(mat, 0, sizeof(lnn_ternary_matrix_t));
    mat->magic = LNN_TERNARY_MAGIC;
    mat->rows = rows;
    mat->cols = cols;
    mat->nnz = 0;
    mat->threshold = LNN_TERNARY_DEFAULT_THRESHOLD;
    mat->scale_factor = 1.0f;
    mat->sparsity = 1.0f;  /* All zeros initially */
    mat->pack_mode = pack_mode;
    mat->is_sparse = use_sparse;

    if (use_sparse) {
        /* Allocate row pointers (always needed) */
        mat->row_ptr = nimcp_malloc((rows + 1) * sizeof(uint32_t));
        if (!mat->row_ptr) {
            nimcp_free(mat);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_matrix_create: mat->row_ptr is NULL");
            return NULL;
        }
        memset(mat->row_ptr, 0, (rows + 1) * sizeof(uint32_t));
        mat->col_idx = NULL;
        mat->signs = NULL;
    } else {
        /* Allocate dense matrix */
        mat->dense = trit_matrix_create(rows, cols, pack_mode);
        if (!mat->dense) {
            nimcp_free(mat);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_matrix_create: mat->dense is NULL");
            return NULL;
        }
    }

    return mat;
}

lnn_ternary_matrix_t* lnn_ternary_matrix_from_float(
    const nimcp_tensor_t* float_weights,
    float threshold,
    ternary_pack_mode_t pack_mode,
    bool use_sparse
) {
    /* Guard: validate inputs */
    if (!float_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL float_weights in lnn_ternary_matrix_from_float");
        return NULL;
    }

    /* Validate threshold */
    if (threshold < LNN_TERNARY_MIN_THRESHOLD) {
        threshold = LNN_TERNARY_MIN_THRESHOLD;
    }
    if (threshold > LNN_TERNARY_MAX_THRESHOLD) {
        threshold = LNN_TERNARY_MAX_THRESHOLD;
    }

    /* Get tensor dimensions (must be 2D) */
    uint32_t rank = nimcp_tensor_rank(float_weights);
    if (rank != 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_DIMENSION_MISMATCH,
                             "Expected 2D tensor, got rank %u", rank);
        return NULL;
    }

    const uint32_t* dims = nimcp_tensor_dims(float_weights);
    uint32_t rows = dims[0];
    uint32_t cols = dims[1];

    /* Compute statistics */
    float scale = compute_scale_factor(float_weights);
    float normalized_threshold = threshold * scale;
    uint32_t nnz = count_nonzeros(float_weights, normalized_threshold);
    float sparsity = 1.0f - (float)nnz / (float)(rows * cols);

    /* Decide sparse vs dense based on actual sparsity */
    bool actual_sparse = use_sparse && (sparsity > 0.5f);

    /* Create matrix */
    lnn_ternary_matrix_t* mat = lnn_ternary_matrix_create(
        rows, cols, pack_mode, actual_sparse
    );
    if (!mat) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_matrix_from_float: mat is NULL");
        return NULL;
    }

    mat->threshold = threshold;
    mat->scale_factor = scale;
    mat->sparsity = sparsity;
    mat->nnz = nnz;

    const float* data = nimcp_tensor_data_const(float_weights);

    if (actual_sparse) {
        /* Allocate sparse arrays */
        if (nnz > 0) {
            mat->col_idx = nimcp_malloc(nnz * sizeof(uint32_t));
            mat->signs = nimcp_malloc(nnz * sizeof(trit_t));
            if (!mat->col_idx || !mat->signs) {
                lnn_ternary_matrix_destroy(mat);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_matrix_from_float: required parameter is NULL (mat->col_idx, mat->signs)");
                return NULL;
            }
        }

        /* Populate CSR structure */
        uint32_t idx = 0;
        for (uint32_t r = 0; r < rows; r++) {
            mat->row_ptr[r] = idx;
            for (uint32_t c = 0; c < cols; c++) {
                float w = data[r * cols + c];
                if (fabsf(w) >= normalized_threshold) {
                    mat->col_idx[idx] = c;
                    mat->signs[idx] = (w > 0) ? TRIT_POSITIVE : TRIT_NEGATIVE;
                    idx++;
                }
            }
        }
        mat->row_ptr[rows] = idx;
    } else {
        /* Populate dense matrix */
        for (uint32_t r = 0; r < rows; r++) {
            for (uint32_t c = 0; c < cols; c++) {
                float w = data[r * cols + c];
                trit_t trit;
                if (fabsf(w) < normalized_threshold) {
                    trit = TRIT_UNKNOWN;
                } else {
                    trit = (w > 0) ? TRIT_POSITIVE : TRIT_NEGATIVE;
                }
                trit_matrix_set(mat->dense, r, c, trit);
            }
        }
    }

    return mat;
}

void lnn_ternary_matrix_destroy(lnn_ternary_matrix_t* mat) {
    if (!mat) return;
    if (mat->magic != LNN_TERNARY_MAGIC) return;

    if (mat->is_sparse) {
        if (mat->row_ptr) nimcp_free(mat->row_ptr);
        if (mat->col_idx) nimcp_free(mat->col_idx);
        if (mat->signs) nimcp_free(mat->signs);
    } else {
        if (mat->dense) trit_matrix_destroy(mat->dense);
    }

    mat->magic = 0;
    nimcp_free(mat);
}

lnn_ternary_matrix_t* lnn_ternary_matrix_clone(const lnn_ternary_matrix_t* src) {
    if (!src || src->magic != LNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_ternary_matrix_clone: src is NULL");
        return NULL;
    }

    lnn_ternary_matrix_t* dst = nimcp_malloc(sizeof(lnn_ternary_matrix_t));
    if (!dst) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_matrix_clone: dst is NULL");
        return NULL;
    }

    /* Copy structure */
    memcpy(dst, src, sizeof(lnn_ternary_matrix_t));

    if (src->is_sparse) {
        /* Clone sparse arrays */
        dst->row_ptr = nimcp_malloc((src->rows + 1) * sizeof(uint32_t));
        if (!dst->row_ptr) {
            nimcp_free(dst);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_matrix_clone: dst->row_ptr is NULL");
            return NULL;
        }
        memcpy(dst->row_ptr, src->row_ptr, (src->rows + 1) * sizeof(uint32_t));

        if (src->nnz > 0) {
            dst->col_idx = nimcp_malloc(src->nnz * sizeof(uint32_t));
            dst->signs = nimcp_malloc(src->nnz * sizeof(trit_t));
            if (!dst->col_idx || !dst->signs) {
                nimcp_free(dst->row_ptr);
                if (dst->col_idx) nimcp_free(dst->col_idx);
                nimcp_free(dst);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_matrix_clone: validation failed");
                return NULL;
            }
            memcpy(dst->col_idx, src->col_idx, src->nnz * sizeof(uint32_t));
            memcpy(dst->signs, src->signs, src->nnz * sizeof(trit_t));
        } else {
            dst->col_idx = NULL;
            dst->signs = NULL;
        }
    } else {
        /* Clone dense matrix */
        dst->dense = trit_matrix_clone(src->dense);
        if (!dst->dense) {
            nimcp_free(dst);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_ternary_matrix_clone: dst->dense is NULL");
            return NULL;
        }
    }

    return dst;
}

/*=============================================================================
 * Matrix Operations
 *===========================================================================*/

int lnn_ternary_matmul(
    const lnn_ternary_matrix_t* mat,
    const nimcp_tensor_t* x,
    nimcp_tensor_t* y
) {
    /* Guard: validate inputs */
    if (!mat || mat->magic != LNN_TERNARY_MAGIC) {
        return LNN_ERROR_NULL_POINTER;
    }
    if (!x || !y) {
        return LNN_ERROR_NULL_POINTER;
    }

    /* Validate dimensions */
    size_t x_len = nimcp_tensor_numel(x);
    size_t y_len = nimcp_tensor_numel(y);
    if (x_len != mat->cols || y_len != mat->rows) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_DIMENSION_MISMATCH,
                             "Dimension mismatch: mat[%u,%u], x[%zu], y[%zu]",
                             mat->rows, mat->cols, x_len, y_len);
        return LNN_ERROR_INVALID_DIMENSION;
    }

    const float* x_data = nimcp_tensor_data_const(x);
    float* y_data = nimcp_tensor_data(y);
    float scale = mat->scale_factor;

    if (mat->is_sparse) {
        /* Sparse CSR matmul */
        for (uint32_t r = 0; r < mat->rows; r++) {
            float sum = 0.0f;
            uint32_t start = mat->row_ptr[r];
            uint32_t end = mat->row_ptr[r + 1];

            for (uint32_t j = start; j < end; j++) {
                uint32_t c = mat->col_idx[j];
                trit_t sign = mat->signs[j];
                /* Branchless sign multiply: sum += sign * x[c] */
                sum += sign * x_data[c];
            }

            y_data[r] = scale * sum;
        }
    } else {
        /* Dense matmul using trit_matrix */
        for (uint32_t r = 0; r < mat->rows; r++) {
            float sum = 0.0f;
            for (uint32_t c = 0; c < mat->cols; c++) {
                trit_t t = trit_matrix_get(mat->dense, r, c);
                sum += t * x_data[c];
            }
            y_data[r] = scale * sum;
        }
    }

    return LNN_SUCCESS;
}

int lnn_ternary_matmul_int(
    const lnn_ternary_matrix_t* mat,
    const nimcp_tensor_t* x,
    int32_t* out
) {
    /* Guard: validate inputs */
    if (!mat || mat->magic != LNN_TERNARY_MAGIC) {
        return LNN_ERROR_NULL_POINTER;
    }
    if (!x || !out) {
        return LNN_ERROR_NULL_POINTER;
    }

    const float* x_data = nimcp_tensor_data_const(x);

    if (mat->is_sparse) {
        for (uint32_t r = 0; r < mat->rows; r++) {
            int32_t sum = 0;
            uint32_t start = mat->row_ptr[r];
            uint32_t end = mat->row_ptr[r + 1];

            for (uint32_t j = start; j < end; j++) {
                uint32_t c = mat->col_idx[j];
                trit_t sign = mat->signs[j];
                /* Round float to nearest int for ternary dot product */
                int32_t x_int = (int32_t)(x_data[c] + 0.5f);
                sum += sign * x_int;
            }

            out[r] = sum;
        }
    } else {
        for (uint32_t r = 0; r < mat->rows; r++) {
            int32_t sum = 0;
            for (uint32_t c = 0; c < mat->cols; c++) {
                trit_t t = trit_matrix_get(mat->dense, r, c);
                int32_t x_int = (int32_t)(x_data[c] + 0.5f);
                sum += t * x_int;
            }
            out[r] = sum;
        }
    }

    return LNN_SUCCESS;
}

nimcp_tensor_t* lnn_ternary_matrix_to_float(const lnn_ternary_matrix_t* mat) {
    if (!mat || mat->magic != LNN_TERNARY_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_ternary_matrix_to_float: mat is NULL");
        return NULL;
    }

    /* Create output tensor */
    uint32_t dims[2] = {mat->rows, mat->cols};
    nimcp_tensor_t* tensor = nimcp_tensor_create(dims, 2, NIMCP_DTYPE_F32);
    if (!tensor) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "lnn_ternary_matrix_to_float: tensor is NULL");
        return NULL;
    }

    float* data = nimcp_tensor_data(tensor);
    float scale = mat->scale_factor;

    /* Initialize to zero */
    memset(data, 0, mat->rows * mat->cols * sizeof(float));

    if (mat->is_sparse) {
        /* Convert sparse to float */
        for (uint32_t r = 0; r < mat->rows; r++) {
            uint32_t start = mat->row_ptr[r];
            uint32_t end = mat->row_ptr[r + 1];

            for (uint32_t j = start; j < end; j++) {
                uint32_t c = mat->col_idx[j];
                data[r * mat->cols + c] = scale * mat->signs[j];
            }
        }
    } else {
        /* Convert dense to float */
        for (uint32_t r = 0; r < mat->rows; r++) {
            for (uint32_t c = 0; c < mat->cols; c++) {
                trit_t t = trit_matrix_get(mat->dense, r, c);
                data[r * mat->cols + c] = scale * t;
            }
        }
    }

    return tensor;
}

trit_t lnn_ternary_matrix_get(
    const lnn_ternary_matrix_t* mat,
    uint32_t row,
    uint32_t col
) {
    if (!mat || mat->magic != LNN_TERNARY_MAGIC) return TRIT_UNKNOWN;
    if (row >= mat->rows || col >= mat->cols) return TRIT_UNKNOWN;

    if (mat->is_sparse) {
        /* Binary search in row's columns */
        uint32_t start = mat->row_ptr[row];
        uint32_t end = mat->row_ptr[row + 1];

        int32_t idx = sparse_find_col(mat->col_idx, start, end, col);
        if (idx >= 0) {
            return mat->signs[idx];
        }
        return TRIT_UNKNOWN;  /* Zero (not stored) */
    } else {
        return trit_matrix_get(mat->dense, row, col);
    }
}

int lnn_ternary_matrix_set(
    lnn_ternary_matrix_t* mat,
    uint32_t row,
    uint32_t col,
    trit_t value
) {
    if (!mat || mat->magic != LNN_TERNARY_MAGIC) {
        return LNN_ERROR_NULL_POINTER;
    }
    if (row >= mat->rows || col >= mat->cols) {
        return LNN_ERROR_INVALID_DIMENSION;
    }
    if (!TRIT_IS_VALID(value)) {
        return LNN_ERROR_INVALID_PARAM;
    }

    if (!mat->is_sparse) {
        /* Dense: direct set */
        return trit_matrix_set(mat->dense, row, col, value);
    }

    /* Sparse: more complex - find and update or error */
    uint32_t start = mat->row_ptr[row];
    uint32_t end = mat->row_ptr[row + 1];

    int32_t idx = sparse_find_col(mat->col_idx, start, end, col);

    if (idx >= 0) {
        /* Found existing entry */
        if (value == TRIT_UNKNOWN) {
            /* Setting to zero - would require structure rebuild */
            NIMCP_LOGGING_WARN("Cannot set sparse entry to zero without rebuild");
            return LNN_ERROR_OPERATION_FAILED;
        }
        mat->signs[idx] = value;
        return LNN_SUCCESS;
    } else {
        /* Entry doesn't exist */
        if (value == TRIT_UNKNOWN) {
            /* Already zero, no-op */
            return LNN_SUCCESS;
        }
        /* Would need to insert - not supported for sparse */
        NIMCP_LOGGING_WARN("Cannot insert new entry in sparse matrix");
        return LNN_ERROR_OPERATION_FAILED;
    }
}

/*=============================================================================
 * Statistics
 *===========================================================================*/

void lnn_ternary_matrix_stats(
    const lnn_ternary_matrix_t* mat,
    uint32_t* n_positive,
    uint32_t* n_zero,
    uint32_t* n_negative
) {
    uint32_t pos = 0, zero = 0, neg = 0;

    if (mat && mat->magic == LNN_TERNARY_MAGIC) {
        uint32_t total = mat->rows * mat->cols;

        if (mat->is_sparse) {
            /* Sparse: count stored signs, rest are zeros */
            for (uint32_t i = 0; i < mat->nnz; i++) {
                if (mat->signs[i] == TRIT_POSITIVE) pos++;
                else if (mat->signs[i] == TRIT_NEGATIVE) neg++;
            }
            zero = total - mat->nnz;
        } else {
            /* Dense: count all elements */
            for (uint32_t r = 0; r < mat->rows; r++) {
                for (uint32_t c = 0; c < mat->cols; c++) {
                    trit_t t = trit_matrix_get(mat->dense, r, c);
                    if (t == TRIT_POSITIVE) pos++;
                    else if (t == TRIT_NEGATIVE) neg++;
                    else zero++;
                }
            }
        }
    }

    if (n_positive) *n_positive = pos;
    if (n_zero) *n_zero = zero;
    if (n_negative) *n_negative = neg;
}

float lnn_ternary_matrix_sparsity(const lnn_ternary_matrix_t* mat) {
    if (!mat || mat->magic != LNN_TERNARY_MAGIC) return 0.0f;

    uint32_t n_zero;
    lnn_ternary_matrix_stats(mat, NULL, &n_zero, NULL);

    uint32_t total = mat->rows * mat->cols;
    return (total > 0) ? (float)n_zero / (float)total : 0.0f;
}

/*=============================================================================
 * Configuration
 *===========================================================================*/

void lnn_ternary_config_default(lnn_ternary_config_t* config) {
    if (!config) return;

    config->enabled = false;
    config->pack_mode = TERNARY_PACK_BASE243;
    config->threshold = LNN_TERNARY_DEFAULT_THRESHOLD;
    config->use_learned_threshold = false;
    config->scale_factor = 1.0f;
    config->apply_to_W_in = false;
    config->apply_to_W_rec = true;   /* Default: only recurrent weights */
    config->apply_to_W_tau = false;
}

int lnn_ternary_config_validate(const lnn_ternary_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                             "NULL config in lnn_ternary_config_validate");
        return LNN_ERROR_NULL_POINTER;
    }

    if (config->threshold < LNN_TERNARY_MIN_THRESHOLD ||
        config->threshold > LNN_TERNARY_MAX_THRESHOLD) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid threshold: %.4f (must be in [%.4f, %.4f])",
                             config->threshold,
                             LNN_TERNARY_MIN_THRESHOLD,
                             LNN_TERNARY_MAX_THRESHOLD);
        return LNN_ERROR_INVALID_CONFIG;
    }

    if (config->scale_factor <= 0.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
                             "Invalid scale_factor: %.4f (must be > 0)",
                             config->scale_factor);
        return LNN_ERROR_INVALID_CONFIG;
    }

    return LNN_SUCCESS;
}

/*=============================================================================
 * Layer Integration (Stub - requires layer structure modification)
 *===========================================================================*/

bool lnn_layer_is_ternary(const lnn_layer_t* layer) {
    /* TODO: Check layer's ternary config flag */
    (void)layer;
    return false;
}

const lnn_ternary_matrix_t* lnn_layer_get_ternary_W_rec(const lnn_layer_t* layer) {
    /* TODO: Return layer's ternary W_rec if enabled */
    (void)layer;
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "lnn_layer_get_ternary_W_rec: operation failed");
    return NULL;
}
