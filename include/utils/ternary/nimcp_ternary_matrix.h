//=============================================================================
// nimcp_ternary_matrix.h - Ternary Matrix Operations
//=============================================================================
/**
 * @file nimcp_ternary_matrix.h
 * @brief Matrix operations for ternary (three-valued) data
 *
 * WHAT: 2D matrix of ternary values with linear algebra operations
 * WHY:  Enable ternary weight matrices for SNN and neural networks
 * HOW:  Row-major storage with optional packing
 *
 * BIOLOGICAL BASIS:
 * - Synaptic weight matrices between neural populations
 * - Connectivity matrices in brain regions
 * - Ternary encoding reduces memory by 5-20x
 *
 * MEMORY SAVINGS EXAMPLE:
 * - 1000x1000 weight matrix
 * - Float32: 4 MB
 * - Ternary unpacked: 1 MB (4x savings)
 * - Ternary base-243: 200 KB (20x savings)
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_TERNARY_MATRIX_H
#define NIMCP_TERNARY_MATRIX_H

#include "nimcp_ternary_types.h"
#include "nimcp_ternary_logic.h"
#include "nimcp_ternary_storage.h"
#include "nimcp_ternary_vector.h"
#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Matrix Structure
//=============================================================================

/**
 * @brief Ternary matrix container
 *
 * WHAT: 2D matrix of ternary values
 * WHY:  Efficient ternary weight matrices for neural networks
 * HOW:  Row-major storage with optional packing
 */
typedef struct {
    uint32_t magic;                 /**< Validation magic number */
    size_t rows;                    /**< Number of rows */
    size_t cols;                    /**< Number of columns */
    size_t numel;                   /**< Total elements (rows * cols) */
    ternary_pack_mode_t pack_mode;  /**< Storage packing mode */

    union {
        trit_t* unpacked;           /**< Unpacked storage (1 byte/trit) */
        uint8_t* packed;            /**< Packed storage (4 or 5 trits/byte) */
    } data;

    size_t packed_bytes;            /**< Bytes used in packed mode */
    size_t row_stride;              /**< Stride between rows (in trits) */
    bool owns_data;                 /**< Whether matrix owns data memory */
} trit_matrix_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new ternary matrix
 *
 * @param rows Number of rows
 * @param cols Number of columns
 * @param pack_mode Storage packing mode
 * @return Pointer to matrix, or NULL on failure
 */
static inline trit_matrix_t* trit_matrix_create(size_t rows, size_t cols,
                                                 ternary_pack_mode_t pack_mode) {
    trit_matrix_t* mat = (trit_matrix_t*)nimcp_malloc(sizeof(trit_matrix_t));
    if (!mat) return NULL;

    mat->magic = TERNARY_MAGIC;
    mat->rows = rows;
    mat->cols = cols;
    mat->numel = rows * cols;
    mat->pack_mode = pack_mode;
    mat->row_stride = cols;
    mat->owns_data = true;

    size_t bytes = trit_packed_bytes(mat->numel, pack_mode);
    mat->packed_bytes = bytes;

    if (pack_mode == TERNARY_PACK_NONE) {
        mat->data.unpacked = (trit_t*)nimcp_calloc(mat->numel, sizeof(trit_t));
        if (!mat->data.unpacked) {
            nimcp_free(mat);
            return NULL;
        }
    } else {
        mat->data.packed = (uint8_t*)nimcp_calloc(bytes, 1);
        if (!mat->data.packed) {
            nimcp_free(mat);
            return NULL;
        }
    }

    return mat;
}

/**
 * @brief Create matrix initialized to constant value
 *
 * @param rows Number of rows
 * @param cols Number of columns
 * @param value Initial value for all elements
 * @param pack_mode Storage packing mode
 * @return Pointer to matrix, or NULL on failure
 */
static inline trit_matrix_t* trit_matrix_create_filled(size_t rows, size_t cols,
                                                        trit_t value,
                                                        ternary_pack_mode_t pack_mode) {
    trit_matrix_t* mat = trit_matrix_create(rows, cols, pack_mode);
    if (!mat) return NULL;

    if (pack_mode == TERNARY_PACK_NONE) {
        for (size_t i = 0; i < mat->numel; i++) {
            mat->data.unpacked[i] = value;
        }
    } else if (pack_mode == TERNARY_PACK_2BIT) {
        trit_t trits[4] = {value, value, value, value};
        trit_packed2_t packed = trit_pack4(trits);
        memset(mat->data.packed, packed, mat->packed_bytes);
    } else {
        trit_t trits[5] = {value, value, value, value, value};
        trit_packed243_t packed = trit_pack5(trits);
        memset(mat->data.packed, packed, mat->packed_bytes);
    }

    return mat;
}

/**
 * @brief Create identity-like matrix (diagonal = POSITIVE, rest = UNKNOWN)
 *
 * @param size Matrix dimension (square)
 * @param pack_mode Storage packing mode
 * @return Pointer to matrix, or NULL on failure
 */
static inline trit_matrix_t* trit_matrix_create_identity(size_t size,
                                                          ternary_pack_mode_t pack_mode) {
    trit_matrix_t* mat = trit_matrix_create(size, size, pack_mode);
    if (!mat) return NULL;

    for (size_t i = 0; i < size; i++) {
        /* Set diagonal to POSITIVE */
        size_t idx = i * size + i;
        if (pack_mode == TERNARY_PACK_NONE) {
            mat->data.unpacked[idx] = TRIT_POSITIVE;
        } else if (pack_mode == TERNARY_PACK_2BIT) {
            trit_t trits[4];
            size_t byte_idx = idx / 4;
            trit_unpack4(mat->data.packed[byte_idx], trits);
            trits[idx % 4] = TRIT_POSITIVE;
            mat->data.packed[byte_idx] = trit_pack4(trits);
        } else {
            trit_t trits[5];
            size_t byte_idx = idx / 5;
            trit_unpack5(mat->data.packed[byte_idx], trits);
            trits[idx % 5] = TRIT_POSITIVE;
            mat->data.packed[byte_idx] = trit_pack5(trits);
        }
    }

    return mat;
}

/**
 * @brief Destroy a ternary matrix
 *
 * @param mat Matrix to destroy
 */
static inline void trit_matrix_destroy(trit_matrix_t* mat) {
    if (!mat) return;
    if (mat->magic != TERNARY_MAGIC) return;

    if (mat->owns_data) {
        if (mat->pack_mode == TERNARY_PACK_NONE) {
            nimcp_free(mat->data.unpacked);
        } else {
            nimcp_free(mat->data.packed);
        }
    }

    mat->magic = 0;
    nimcp_free(mat);
}

/**
 * @brief Clone a ternary matrix
 *
 * @param src Source matrix
 * @return New matrix copy, or NULL on failure
 */
static inline trit_matrix_t* trit_matrix_clone(const trit_matrix_t* src) {
    if (!src || src->magic != TERNARY_MAGIC) return NULL;

    trit_matrix_t* dst = trit_matrix_create(src->rows, src->cols, src->pack_mode);
    if (!dst) return NULL;

    if (src->pack_mode == TERNARY_PACK_NONE) {
        memcpy(dst->data.unpacked, src->data.unpacked, src->numel * sizeof(trit_t));
    } else {
        memcpy(dst->data.packed, src->data.packed, src->packed_bytes);
    }

    return dst;
}

//=============================================================================
// Fill Operations
//=============================================================================

/**
 * @brief Fill matrix with constant value (in-place)
 *
 * @param mat Matrix to fill
 * @param value Value to fill with
 * @return TERNARY_OK on success
 */
static inline ternary_error_t trit_matrix_fill(trit_matrix_t* mat, trit_t value) {
    if (!mat || mat->magic != TERNARY_MAGIC) return TERNARY_ERR_NULL;
    if (!TRIT_IS_VALID(value)) return TERNARY_ERR_INVALID;

    if (mat->pack_mode == TERNARY_PACK_NONE) {
        for (size_t i = 0; i < mat->numel; i++) {
            mat->data.unpacked[i] = value;
        }
    } else if (mat->pack_mode == TERNARY_PACK_2BIT) {
        trit_t trits[4] = {value, value, value, value};
        trit_packed2_t packed = trit_pack4(trits);
        memset(mat->data.packed, packed, mat->packed_bytes);
    } else {
        trit_t trits[5] = {value, value, value, value, value};
        trit_packed243_t packed = trit_pack5(trits);
        memset(mat->data.packed, packed, mat->packed_bytes);
    }

    return TERNARY_OK;
}

//=============================================================================
// Element Access
//=============================================================================

/**
 * @brief Get element at (row, col)
 *
 * @param mat Matrix
 * @param row Row index
 * @param col Column index
 * @return Trit value, or TRIT_UNKNOWN if invalid
 */
static inline trit_t trit_matrix_get(const trit_matrix_t* mat, size_t row, size_t col) {
    if (!mat || mat->magic != TERNARY_MAGIC) return TRIT_UNKNOWN;
    if (row >= mat->rows || col >= mat->cols) return TRIT_UNKNOWN;

    size_t idx = row * mat->cols + col;

    if (mat->pack_mode == TERNARY_PACK_NONE) {
        return mat->data.unpacked[idx];
    } else if (mat->pack_mode == TERNARY_PACK_2BIT) {
        trit_t trits[4];
        trit_unpack4(mat->data.packed[idx / 4], trits);
        return trits[idx % 4];
    } else {
        trit_t trits[5];
        trit_unpack5(mat->data.packed[idx / 5], trits);
        return trits[idx % 5];
    }
}

/**
 * @brief Set element at (row, col)
 *
 * @param mat Matrix
 * @param row Row index
 * @param col Column index
 * @param value New value
 * @return TERNARY_OK on success
 */
static inline ternary_error_t trit_matrix_set(trit_matrix_t* mat, size_t row, size_t col,
                                               trit_t value) {
    if (!mat || mat->magic != TERNARY_MAGIC) return TERNARY_ERR_NULL;
    if (row >= mat->rows || col >= mat->cols) return TERNARY_ERR_INDEX;
    if (!TRIT_IS_VALID(value)) return TERNARY_ERR_INVALID;

    size_t idx = row * mat->cols + col;

    if (mat->pack_mode == TERNARY_PACK_NONE) {
        mat->data.unpacked[idx] = value;
    } else if (mat->pack_mode == TERNARY_PACK_2BIT) {
        trit_t trits[4];
        size_t byte_idx = idx / 4;
        trit_unpack4(mat->data.packed[byte_idx], trits);
        trits[idx % 4] = value;
        mat->data.packed[byte_idx] = trit_pack4(trits);
    } else {
        trit_t trits[5];
        size_t byte_idx = idx / 5;
        trit_unpack5(mat->data.packed[byte_idx], trits);
        trits[idx % 5] = value;
        mat->data.packed[byte_idx] = trit_pack5(trits);
    }

    return TERNARY_OK;
}

//=============================================================================
// Row/Column Operations
//=============================================================================

/**
 * @brief Get a row as a vector
 *
 * @param mat Source matrix
 * @param row Row index
 * @return New vector containing row data, or NULL on failure
 */
static inline trit_vector_t* trit_matrix_get_row(const trit_matrix_t* mat, size_t row) {
    if (!mat || mat->magic != TERNARY_MAGIC || row >= mat->rows) return NULL;

    trit_vector_t* vec = trit_vector_create(mat->cols, TERNARY_PACK_NONE);
    if (!vec) return NULL;

    for (size_t col = 0; col < mat->cols; col++) {
        vec->data.unpacked[col] = trit_matrix_get(mat, row, col);
    }

    return vec;
}

/**
 * @brief Get a column as a vector
 *
 * @param mat Source matrix
 * @param col Column index
 * @return New vector containing column data, or NULL on failure
 */
static inline trit_vector_t* trit_matrix_get_col(const trit_matrix_t* mat, size_t col) {
    if (!mat || mat->magic != TERNARY_MAGIC || col >= mat->cols) return NULL;

    trit_vector_t* vec = trit_vector_create(mat->rows, TERNARY_PACK_NONE);
    if (!vec) return NULL;

    for (size_t row = 0; row < mat->rows; row++) {
        vec->data.unpacked[row] = trit_matrix_get(mat, row, col);
    }

    return vec;
}

/**
 * @brief Set a row from a vector
 *
 * @param mat Target matrix
 * @param row Row index
 * @param vec Source vector
 * @return TERNARY_OK on success
 */
static inline ternary_error_t trit_matrix_set_row(trit_matrix_t* mat, size_t row,
                                                   const trit_vector_t* vec) {
    if (!mat || mat->magic != TERNARY_MAGIC) return TERNARY_ERR_NULL;
    if (!vec || vec->magic != TERNARY_MAGIC) return TERNARY_ERR_NULL;
    if (row >= mat->rows) return TERNARY_ERR_INDEX;
    if (vec->length != mat->cols) return TERNARY_ERR_SHAPE;

    for (size_t col = 0; col < mat->cols; col++) {
        trit_matrix_set(mat, row, col, trit_vector_get(vec, col));
    }

    return TERNARY_OK;
}

//=============================================================================
// Matrix Arithmetic
//=============================================================================

/**
 * @brief Matrix-vector multiplication (ternary)
 *
 * Computes y = M * x where multiplication is standard integer
 * and the result is clamped to valid trit range.
 *
 * @param mat Matrix (m x n)
 * @param vec Vector (n elements)
 * @return Result vector (m elements), or NULL on failure
 */
static inline trit_vector_t* trit_matrix_vector_mul(const trit_matrix_t* mat,
                                                     const trit_vector_t* vec) {
    if (!mat || mat->magic != TERNARY_MAGIC) return NULL;
    if (!vec || vec->magic != TERNARY_MAGIC) return NULL;
    if (vec->length != mat->cols) return NULL;

    trit_vector_t* result = trit_vector_create(mat->rows, TERNARY_PACK_NONE);
    if (!result) return NULL;

    for (size_t row = 0; row < mat->rows; row++) {
        int sum = 0;
        for (size_t col = 0; col < mat->cols; col++) {
            sum += trit_matrix_get(mat, row, col) * trit_vector_get(vec, col);
        }
        /* Clamp to valid trit range */
        result->data.unpacked[row] = TRIT_SIGN(sum);
    }

    return result;
}

/**
 * @brief Matrix-vector multiplication returning integer vector
 *
 * Like trit_matrix_vector_mul but returns unbounded integer values.
 *
 * @param mat Matrix (m x n)
 * @param vec Vector (n elements)
 * @param out Output integer array (m elements, caller allocated)
 * @return TERNARY_OK on success
 */
static inline ternary_error_t trit_matrix_vector_mul_int(const trit_matrix_t* mat,
                                                          const trit_vector_t* vec,
                                                          int* out) {
    if (!mat || mat->magic != TERNARY_MAGIC) return TERNARY_ERR_NULL;
    if (!vec || vec->magic != TERNARY_MAGIC) return TERNARY_ERR_NULL;
    if (!out) return TERNARY_ERR_NULL;
    if (vec->length != mat->cols) return TERNARY_ERR_SHAPE;

    for (size_t row = 0; row < mat->rows; row++) {
        int sum = 0;
        for (size_t col = 0; col < mat->cols; col++) {
            sum += trit_matrix_get(mat, row, col) * trit_vector_get(vec, col);
        }
        out[row] = sum;
    }

    return TERNARY_OK;
}

/**
 * @brief Element-wise matrix AND
 *
 * @param a First matrix
 * @param b Second matrix
 * @return Result matrix, or NULL on failure
 */
static inline trit_matrix_t* trit_matrix_and(const trit_matrix_t* a, const trit_matrix_t* b) {
    if (!a || !b || a->magic != TERNARY_MAGIC || b->magic != TERNARY_MAGIC) return NULL;
    if (a->rows != b->rows || a->cols != b->cols) return NULL;

    trit_matrix_t* result = trit_matrix_create(a->rows, a->cols, TERNARY_PACK_NONE);
    if (!result) return NULL;

    for (size_t i = 0; i < a->numel; i++) {
        trit_t va, vb;
        if (a->pack_mode == TERNARY_PACK_NONE) {
            va = a->data.unpacked[i];
        } else {
            va = trit_matrix_get(a, i / a->cols, i % a->cols);
        }
        if (b->pack_mode == TERNARY_PACK_NONE) {
            vb = b->data.unpacked[i];
        } else {
            vb = trit_matrix_get(b, i / b->cols, i % b->cols);
        }
        result->data.unpacked[i] = trit_and(va, vb);
    }

    return result;
}

/**
 * @brief Element-wise matrix OR
 *
 * @param a First matrix
 * @param b Second matrix
 * @return Result matrix, or NULL on failure
 */
static inline trit_matrix_t* trit_matrix_or(const trit_matrix_t* a, const trit_matrix_t* b) {
    if (!a || !b || a->magic != TERNARY_MAGIC || b->magic != TERNARY_MAGIC) return NULL;
    if (a->rows != b->rows || a->cols != b->cols) return NULL;

    trit_matrix_t* result = trit_matrix_create(a->rows, a->cols, TERNARY_PACK_NONE);
    if (!result) return NULL;

    for (size_t i = 0; i < a->numel; i++) {
        trit_t va, vb;
        if (a->pack_mode == TERNARY_PACK_NONE) {
            va = a->data.unpacked[i];
        } else {
            va = trit_matrix_get(a, i / a->cols, i % a->cols);
        }
        if (b->pack_mode == TERNARY_PACK_NONE) {
            vb = b->data.unpacked[i];
        } else {
            vb = trit_matrix_get(b, i / b->cols, i % b->cols);
        }
        result->data.unpacked[i] = trit_or(va, vb);
    }

    return result;
}

/**
 * @brief Transpose matrix
 *
 * @param mat Input matrix
 * @return Transposed matrix, or NULL on failure
 */
static inline trit_matrix_t* trit_matrix_transpose(const trit_matrix_t* mat) {
    if (!mat || mat->magic != TERNARY_MAGIC) return NULL;

    trit_matrix_t* result = trit_matrix_create(mat->cols, mat->rows, TERNARY_PACK_NONE);
    if (!result) return NULL;

    for (size_t row = 0; row < mat->rows; row++) {
        for (size_t col = 0; col < mat->cols; col++) {
            result->data.unpacked[col * mat->rows + row] = trit_matrix_get(mat, row, col);
        }
    }

    return result;
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Count matrix elements by value
 *
 * @param mat Input matrix
 * @param n_positive Output count of +1 values
 * @param n_unknown Output count of 0 values
 * @param n_negative Output count of -1 values
 */
static inline void trit_matrix_count(const trit_matrix_t* mat,
                                      size_t* n_positive,
                                      size_t* n_unknown,
                                      size_t* n_negative) {
    size_t pos = 0, unk = 0, neg = 0;

    if (mat && mat->magic == TERNARY_MAGIC) {
        for (size_t i = 0; i < mat->numel; i++) {
            trit_t t;
            if (mat->pack_mode == TERNARY_PACK_NONE) {
                t = mat->data.unpacked[i];
            } else {
                t = trit_matrix_get(mat, i / mat->cols, i % mat->cols);
            }
            if (t == TRIT_POSITIVE) pos++;
            else if (t == TRIT_NEGATIVE) neg++;
            else unk++;
        }
    }

    if (n_positive) *n_positive = pos;
    if (n_unknown) *n_unknown = unk;
    if (n_negative) *n_negative = neg;
}

/**
 * @brief Compute sparsity (fraction of UNKNOWN elements)
 *
 * @param mat Input matrix
 * @return Sparsity in [0,1]
 */
static inline float trit_matrix_sparsity(const trit_matrix_t* mat) {
    if (!mat || mat->magic != TERNARY_MAGIC || mat->numel == 0) return 0.0f;

    size_t n_unknown;
    trit_matrix_count(mat, NULL, &n_unknown, NULL);

    return (float)n_unknown / (float)mat->numel;
}

/**
 * @brief Get memory size of a ternary matrix in bytes
 *
 * @param mat Input matrix
 * @return Memory size in bytes (data + overhead)
 */
static inline size_t trit_matrix_memory_size(const trit_matrix_t* mat) {
    if (!mat || mat->magic != TERNARY_MAGIC) return 0;

    size_t data_size;
    if (mat->pack_mode == TERNARY_PACK_NONE) {
        data_size = mat->numel * sizeof(trit_t);
    } else if (mat->pack_mode == TERNARY_PACK_2BIT) {
        data_size = (mat->numel + 3) / 4;  // 4 trits per byte
    } else {
        data_size = (mat->numel + 4) / 5;  // 5 trits per byte
    }

    return sizeof(trit_matrix_t) + data_size;
}

//=============================================================================
// Pack Mode Conversion
//=============================================================================

/**
 * @brief Convert matrix to different pack mode
 *
 * @param mat Source matrix
 * @param new_mode Target packing mode
 * @return New matrix in specified mode, or NULL on failure
 */
static inline trit_matrix_t* trit_matrix_convert(const trit_matrix_t* mat,
                                                  ternary_pack_mode_t new_mode) {
    if (!mat || mat->magic != TERNARY_MAGIC) return NULL;

    trit_matrix_t* result = trit_matrix_create(mat->rows, mat->cols, new_mode);
    if (!result) return NULL;

    for (size_t row = 0; row < mat->rows; row++) {
        for (size_t col = 0; col < mat->cols; col++) {
            trit_matrix_set(result, row, col, trit_matrix_get(mat, row, col));
        }
    }

    return result;
}

//=============================================================================
// Serialization
//=============================================================================

/**
 * @brief Serialize matrix to buffer
 *
 * @param mat Matrix to serialize
 * @param buffer Output buffer (NULL to just get size)
 * @param buffer_size Size of buffer
 * @return Required buffer size, or 0 on error
 */
static inline size_t trit_matrix_serialize(const trit_matrix_t* mat,
                                            uint8_t* buffer,
                                            size_t buffer_size) {
    if (!mat || mat->magic != TERNARY_MAGIC) return 0;

    // Calculate required size: header (magic, rows, cols, pack_mode) + data
    size_t data_size;
    if (mat->pack_mode == TERNARY_PACK_NONE) {
        data_size = mat->numel * sizeof(trit_t);
    } else if (mat->pack_mode == TERNARY_PACK_2BIT) {
        data_size = (mat->numel + 3) / 4;
    } else {
        data_size = (mat->numel + 4) / 5;
    }

    size_t header_size = sizeof(uint32_t) * 4;  // magic, rows, cols, pack_mode
    size_t total_size = header_size + data_size;

    // Just return size if no buffer
    if (!buffer) return total_size;
    if (buffer_size < total_size) return 0;

    // Write header
    uint32_t* header = (uint32_t*)buffer;
    header[0] = mat->magic;
    header[1] = (uint32_t)mat->rows;
    header[2] = (uint32_t)mat->cols;
    header[3] = (uint32_t)mat->pack_mode;

    // Write data
    uint8_t* data_ptr = buffer + header_size;
    if (mat->pack_mode == TERNARY_PACK_NONE) {
        memcpy(data_ptr, mat->data.unpacked, data_size);
    } else {
        memcpy(data_ptr, mat->data.packed, data_size);
    }

    return total_size;
}

/**
 * @brief Deserialize matrix from buffer
 *
 * @param buffer Input buffer
 * @param buffer_size Size of buffer
 * @return Deserialized matrix, or NULL on error
 */
static inline trit_matrix_t* trit_matrix_deserialize(const uint8_t* buffer,
                                                      size_t buffer_size) {
    if (!buffer || buffer_size < sizeof(uint32_t) * 4) return NULL;

    // Read header
    const uint32_t* header = (const uint32_t*)buffer;
    uint32_t magic = header[0];
    size_t rows = header[1];
    size_t cols = header[2];
    ternary_pack_mode_t pack_mode = (ternary_pack_mode_t)header[3];

    if (magic != TERNARY_MAGIC) return NULL;

    // Calculate data size
    size_t numel = rows * cols;
    size_t data_size;
    if (pack_mode == TERNARY_PACK_NONE) {
        data_size = numel * sizeof(trit_t);
    } else if (pack_mode == TERNARY_PACK_2BIT) {
        data_size = (numel + 3) / 4;
    } else {
        data_size = (numel + 4) / 5;
    }

    size_t header_size = sizeof(uint32_t) * 4;
    if (buffer_size < header_size + data_size) return NULL;

    // Create matrix
    trit_matrix_t* mat = trit_matrix_create(rows, cols, pack_mode);
    if (!mat) return NULL;

    // Copy data
    const uint8_t* data_ptr = buffer + header_size;
    if (pack_mode == TERNARY_PACK_NONE) {
        memcpy(mat->data.unpacked, data_ptr, data_size);
    } else {
        memcpy(mat->data.packed, data_ptr, data_size);
    }

    return mat;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TERNARY_MATRIX_H */
