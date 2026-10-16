//=============================================================================
// nimcp_ternary_nd.h - N-Dimensional Ternary Tensor
//=============================================================================
/**
 * @file nimcp_ternary_nd.h
 * @brief Native N-dimensional ternary tensor type
 *
 * WHAT: Multi-dimensional array of ternary values {-1, 0, +1}
 * WHY:  Efficient storage for neural network layer states and activations
 * HOW:  Packed storage with arbitrary rank and dimensions
 *
 * MEMORY LAYOUT:
 * - Row-major (C-style) ordering
 * - Packed storage for memory efficiency
 * - Supports up to 8 dimensions
 *
 * USE CASES:
 * - Cortical column layer states
 * - Attention tensor masks
 * - Sparse activation patterns
 *
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#ifndef NIMCP_TERNARY_ND_H
#define NIMCP_TERNARY_ND_H

#include "nimcp_ternary_types.h"
#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum tensor rank (number of dimensions) */
#define TRIT_TENSOR_MAX_RANK 8

/** Magic number for validation */
#define TRIT_TENSOR_MAGIC 0x54524E44  /* "TRND" */

//=============================================================================
// Types
//=============================================================================

/**
 * @brief N-dimensional ternary tensor
 *
 * WHAT: Multi-dimensional array of ternary values
 * WHY:  Native ternary storage for layer states
 * HOW:  Packed or unpacked storage with shape metadata
 */
typedef struct trit_tensor_s {
    uint32_t magic;                      /**< Validation magic */
    uint32_t rank;                       /**< Number of dimensions */
    size_t dims[TRIT_TENSOR_MAX_RANK];   /**< Dimension sizes */
    size_t strides[TRIT_TENSOR_MAX_RANK];/**< Strides for indexing */
    size_t numel;                        /**< Total number of elements */
    ternary_pack_mode_t pack_mode;       /**< Packing mode */

    union {
        trit_t* unpacked;                /**< Unpacked: 1 trit per byte */
        uint8_t* packed;                 /**< Packed: 4 or 5 trits per byte */
    } data;
} trit_tensor_t;

//=============================================================================
// Creation and Destruction
//=============================================================================

/**
 * @brief Create an N-dimensional ternary tensor
 *
 * @param dims Array of dimension sizes
 * @param rank Number of dimensions (1 to TRIT_TENSOR_MAX_RANK)
 * @param pack_mode Packing mode for storage
 * @return New tensor initialized to TRIT_UNKNOWN, or NULL on failure
 */
static inline trit_tensor_t* trit_tensor_create(
    const size_t* dims,
    uint32_t rank,
    ternary_pack_mode_t pack_mode
) {
    if (!dims || rank == 0 || rank > TRIT_TENSOR_MAX_RANK) return NULL;

    trit_tensor_t* tensor = (trit_tensor_t*)nimcp_calloc(1, sizeof(trit_tensor_t));
    if (!tensor) return NULL;

    tensor->magic = TRIT_TENSOR_MAGIC;
    tensor->rank = rank;
    tensor->pack_mode = pack_mode;

    // Copy dimensions and compute numel
    tensor->numel = 1;
    for (uint32_t i = 0; i < rank; i++) {
        tensor->dims[i] = dims[i];
        tensor->numel *= dims[i];
    }

    // Compute strides (row-major)
    tensor->strides[rank - 1] = 1;
    for (int i = (int)rank - 2; i >= 0; i--) {
        tensor->strides[i] = tensor->strides[i + 1] * dims[i + 1];
    }

    // Allocate data
    size_t data_size;
    if (pack_mode == TERNARY_PACK_NONE) {
        data_size = tensor->numel * sizeof(trit_t);
        tensor->data.unpacked = (trit_t*)nimcp_calloc(tensor->numel, sizeof(trit_t));
        if (!tensor->data.unpacked) {
            nimcp_free(tensor);
            return NULL;
        }
        // Initialize to TRIT_UNKNOWN (0)
        memset(tensor->data.unpacked, TRIT_UNKNOWN, tensor->numel);
    } else if (pack_mode == TERNARY_PACK_2BIT) {
        data_size = (tensor->numel + 3) / 4;  // 4 trits per byte
        tensor->data.packed = (uint8_t*)nimcp_calloc(data_size, 1);
        if (!tensor->data.packed) {
            nimcp_free(tensor);
            return NULL;
        }
    } else {  // TERNARY_PACK_BASE243
        data_size = (tensor->numel + 4) / 5;  // 5 trits per byte
        tensor->data.packed = (uint8_t*)nimcp_calloc(data_size, 1);
        if (!tensor->data.packed) {
            nimcp_free(tensor);
            return NULL;
        }
    }

    return tensor;
}

/**
 * @brief Destroy a ternary tensor and free memory
 *
 * @param tensor Tensor to destroy
 */
static inline void trit_tensor_destroy(trit_tensor_t* tensor) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC) return;

    if (tensor->pack_mode == TERNARY_PACK_NONE) {
        nimcp_free(tensor->data.unpacked);
    } else {
        nimcp_free(tensor->data.packed);
    }

    tensor->magic = 0;  // Invalidate
    nimcp_free(tensor);
}

//=============================================================================
// Element Access
//=============================================================================

/**
 * @brief Compute linear index from coordinates
 *
 * @param tensor Tensor
 * @param coords Array of coordinates (same length as rank)
 * @return Linear index, or SIZE_MAX on error
 */
static inline size_t trit_tensor_linear_index(
    const trit_tensor_t* tensor,
    const size_t* coords
) {
    if (!tensor || !coords || tensor->magic != TRIT_TENSOR_MAGIC) return SIZE_MAX;

    size_t idx = 0;
    for (uint32_t i = 0; i < tensor->rank; i++) {
        if (coords[i] >= tensor->dims[i]) return SIZE_MAX;
        idx += coords[i] * tensor->strides[i];
    }
    return idx;
}

/**
 * @brief Get element at coordinates
 *
 * @param tensor Source tensor
 * @param coords Array of coordinates
 * @return Ternary value, or TRIT_UNKNOWN on error
 */
static inline trit_t trit_tensor_get_element(
    const trit_tensor_t* tensor,
    const size_t* coords
) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC) return TRIT_UNKNOWN;

    size_t idx = trit_tensor_linear_index(tensor, coords);
    if (idx == SIZE_MAX) return TRIT_UNKNOWN;

    if (tensor->pack_mode == TERNARY_PACK_NONE) {
        return tensor->data.unpacked[idx];
    } else if (tensor->pack_mode == TERNARY_PACK_2BIT) {
        size_t byte_idx = idx / 4;
        size_t bit_offset = (idx % 4) * 2;
        uint8_t byte = tensor->data.packed[byte_idx];
        return (trit_t)((byte >> bit_offset) & 0x03);
    } else {  // TERNARY_PACK_BASE243
        size_t byte_idx = idx / 5;
        size_t trit_idx = idx % 5;
        trit_t trits[5];
        trit_unpack5(tensor->data.packed[byte_idx], trits);
        return trits[trit_idx];
    }
}

/**
 * @brief Set element at coordinates
 *
 * @param tensor Target tensor
 * @param coords Array of coordinates
 * @param value Ternary value to set
 * @return TERNARY_OK on success, error code on failure
 */
static inline ternary_error_t trit_tensor_set_element(
    trit_tensor_t* tensor,
    const size_t* coords,
    trit_t value
) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC) return TERNARY_ERR_NULL;

    size_t idx = trit_tensor_linear_index(tensor, coords);
    if (idx == SIZE_MAX) return TERNARY_ERR_INDEX;

    if (tensor->pack_mode == TERNARY_PACK_NONE) {
        tensor->data.unpacked[idx] = value;
    } else if (tensor->pack_mode == TERNARY_PACK_2BIT) {
        size_t byte_idx = idx / 4;
        size_t bit_offset = (idx % 4) * 2;
        uint8_t mask = ~(0x03 << bit_offset);
        tensor->data.packed[byte_idx] =
            (tensor->data.packed[byte_idx] & mask) | ((value & 0x03) << bit_offset);
    } else {  // TERNARY_PACK_BASE243
        size_t byte_idx = idx / 5;
        size_t trit_idx = idx % 5;
        trit_t trits[5];
        trit_unpack5(tensor->data.packed[byte_idx], trits);
        trits[trit_idx] = value;
        tensor->data.packed[byte_idx] = trit_pack5(trits);
    }

    return TERNARY_OK;
}

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get memory size of tensor in bytes
 *
 * @param tensor Input tensor
 * @return Memory size in bytes (data + overhead)
 */
static inline size_t trit_tensor_memory_size(const trit_tensor_t* tensor) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC) return 0;

    size_t data_size;
    if (tensor->pack_mode == TERNARY_PACK_NONE) {
        data_size = tensor->numel * sizeof(trit_t);
    } else if (tensor->pack_mode == TERNARY_PACK_2BIT) {
        data_size = (tensor->numel + 3) / 4;
    } else {
        data_size = (tensor->numel + 4) / 5;
    }

    return sizeof(trit_tensor_t) + data_size;
}

/**
 * @brief Get tensor rank (number of dimensions)
 *
 * @param tensor Input tensor
 * @return Rank, or 0 on error
 */
static inline uint32_t trit_tensor_rank(const trit_tensor_t* tensor) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC) return 0;
    return tensor->rank;
}

/**
 * @brief Get dimension size
 *
 * @param tensor Input tensor
 * @param dim Dimension index
 * @return Dimension size, or 0 on error
 */
static inline size_t trit_tensor_dim(const trit_tensor_t* tensor, uint32_t dim) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC || dim >= tensor->rank) return 0;
    return tensor->dims[dim];
}

/**
 * @brief Get total number of elements
 *
 * @param tensor Input tensor
 * @return Number of elements, or 0 on error
 */
static inline size_t trit_tensor_numel(const trit_tensor_t* tensor) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC) return 0;
    return tensor->numel;
}

/**
 * @brief Fill tensor with a value
 *
 * @param tensor Target tensor
 * @param value Value to fill
 * @return TERNARY_OK on success
 */
static inline ternary_error_t trit_tensor_fill(trit_tensor_t* tensor, trit_t value) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC) return TERNARY_ERR_NULL;

    if (tensor->pack_mode == TERNARY_PACK_NONE) {
        memset(tensor->data.unpacked, value, tensor->numel);
    } else {
        // For packed, we need to set each element
        size_t data_size = (tensor->pack_mode == TERNARY_PACK_2BIT)
            ? (tensor->numel + 3) / 4
            : (tensor->numel + 4) / 5;

        if (value == TRIT_UNKNOWN) {
            // All zeros
            memset(tensor->data.packed, 0, data_size);
        } else if (tensor->pack_mode == TERNARY_PACK_2BIT) {
            // Pack same value 4 times
            uint8_t packed = (value & 0x03) | ((value & 0x03) << 2) |
                            ((value & 0x03) << 4) | ((value & 0x03) << 6);
            memset(tensor->data.packed, packed, data_size);
        } else {
            // Pack same value 5 times
            trit_t trits[5] = {value, value, value, value, value};
            uint8_t packed = trit_pack5(trits);
            memset(tensor->data.packed, packed, data_size);
        }
    }

    return TERNARY_OK;
}

/**
 * @brief Compute sparsity (fraction of TRIT_UNKNOWN values)
 *
 * @param tensor Input tensor
 * @return Sparsity in [0,1], or 0 on error
 */
static inline float trit_nd_sparsity(const trit_tensor_t* tensor) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC || tensor->numel == 0) {
        return 0.0f;
    }

    size_t unknown_count = 0;

    if (tensor->pack_mode == TERNARY_PACK_NONE) {
        for (size_t i = 0; i < tensor->numel; i++) {
            if (tensor->data.unpacked[i] == TRIT_UNKNOWN) {
                unknown_count++;
            }
        }
    } else {
        // For packed, iterate through elements
        for (size_t i = 0; i < tensor->numel; i++) {
            trit_t value;
            if (tensor->pack_mode == TERNARY_PACK_2BIT) {
                size_t byte_idx = i / 4;
                size_t bit_offset = (i % 4) * 2;
                value = (trit_t)((tensor->data.packed[byte_idx] >> bit_offset) & 0x03);
            } else {
                size_t byte_idx = i / 5;
                size_t trit_idx = i % 5;
                trit_t trits[5];
                trit_unpack5(tensor->data.packed[byte_idx], trits);
                value = trits[trit_idx];
            }
            if (value == TRIT_UNKNOWN) {
                unknown_count++;
            }
        }
    }

    return (float)unknown_count / (float)tensor->numel;
}

/**
 * @brief Count values by category
 *
 * @param tensor Input tensor
 * @param n_positive Output: count of TRIT_POSITIVE (can be NULL)
 * @param n_unknown Output: count of TRIT_UNKNOWN (can be NULL)
 * @param n_negative Output: count of TRIT_NEGATIVE (can be NULL)
 */
static inline void trit_tensor_count(
    const trit_tensor_t* tensor,
    size_t* n_positive,
    size_t* n_unknown,
    size_t* n_negative
) {
    size_t pos = 0, unk = 0, neg = 0;

    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC) {
        if (n_positive) *n_positive = 0;
        if (n_unknown) *n_unknown = 0;
        if (n_negative) *n_negative = 0;
        return;
    }

    for (size_t i = 0; i < tensor->numel; i++) {
        trit_t value;
        if (tensor->pack_mode == TERNARY_PACK_NONE) {
            value = tensor->data.unpacked[i];
        } else if (tensor->pack_mode == TERNARY_PACK_2BIT) {
            size_t byte_idx = i / 4;
            size_t bit_offset = (i % 4) * 2;
            value = (trit_t)((tensor->data.packed[byte_idx] >> bit_offset) & 0x03);
        } else {
            size_t byte_idx = i / 5;
            size_t trit_idx = i % 5;
            trit_t trits[5];
            trit_unpack5(tensor->data.packed[byte_idx], trits);
            value = trits[trit_idx];
        }

        switch (value) {
            case TRIT_POSITIVE: pos++; break;
            case TRIT_UNKNOWN:  unk++; break;
            case TRIT_NEGATIVE: neg++; break;
            default: break;
        }
    }

    if (n_positive) *n_positive = pos;
    if (n_unknown) *n_unknown = unk;
    if (n_negative) *n_negative = neg;
}

/**
 * @brief Clone a tensor
 *
 * @param tensor Source tensor
 * @return New tensor with same data, or NULL on failure
 */
static inline trit_tensor_t* trit_tensor_clone(const trit_tensor_t* tensor) {
    if (!tensor || tensor->magic != TRIT_TENSOR_MAGIC) return NULL;

    trit_tensor_t* clone = trit_tensor_create(tensor->dims, tensor->rank, tensor->pack_mode);
    if (!clone) return NULL;

    // Copy data
    size_t data_size;
    if (tensor->pack_mode == TERNARY_PACK_NONE) {
        data_size = tensor->numel * sizeof(trit_t);
        memcpy(clone->data.unpacked, tensor->data.unpacked, data_size);
    } else if (tensor->pack_mode == TERNARY_PACK_2BIT) {
        data_size = (tensor->numel + 3) / 4;
        memcpy(clone->data.packed, tensor->data.packed, data_size);
    } else {
        data_size = (tensor->numel + 4) / 5;
        memcpy(clone->data.packed, tensor->data.packed, data_size);
    }

    return clone;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TERNARY_ND_H */
