//=============================================================================
// nimcp_ternary_vector.h - Ternary Vector Operations
//=============================================================================
/**
 * @file nimcp_ternary_vector.h
 * @brief Vector operations for ternary (three-valued) data
 *
 * WHAT: Dynamic and fixed-size ternary vector operations
 * WHY:  Enable vectorized ternary computation for neural weights and states
 * HOW:  Provides vector type with optional packed storage
 *
 * BIOLOGICAL BASIS:
 * - Neural population codes use distributed representations
 * - Synaptic weight vectors in ternary form (inhibitory/silent/excitatory)
 * - Attention vectors with tri-state gating
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_TERNARY_VECTOR_H
#define NIMCP_TERNARY_VECTOR_H

#include "nimcp_ternary_types.h"
#include "nimcp_ternary_logic.h"
#include "nimcp_ternary_storage.h"
#include <stdlib.h>
#include <string.h>
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Vector Structure
//=============================================================================

/**
 * @brief Ternary vector container
 *
 * WHAT: Dynamically-sized vector of ternary values
 * WHY:  Efficient storage and operations on trit sequences
 * HOW:  Supports unpacked (fast) and packed (memory-efficient) modes
 */
typedef struct {
    uint32_t magic;                 /**< Validation magic number */
    size_t length;                  /**< Number of trits in vector */
    size_t capacity;                /**< Allocated capacity (trits) */
    ternary_pack_mode_t pack_mode;  /**< Storage packing mode */

    union {
        trit_t* unpacked;           /**< Unpacked storage (1 byte/trit) */
        uint8_t* packed;            /**< Packed storage (4 or 5 trits/byte) */
    } data;

    size_t packed_bytes;            /**< Bytes used in packed mode */
    bool owns_data;                 /**< Whether vector owns data memory */
} trit_vector_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Create a new ternary vector
 *
 * @param length Number of trits
 * @param pack_mode Storage packing mode
 * @return Pointer to vector, or NULL on failure
 */
static inline trit_vector_t* trit_vector_create(size_t length, ternary_pack_mode_t pack_mode) {
    trit_vector_t* vec = (trit_vector_t*)nimcp_malloc(sizeof(trit_vector_t));
    if (!vec) return NULL;

    vec->magic = TERNARY_MAGIC;
    vec->length = length;
    vec->capacity = length;
    vec->pack_mode = pack_mode;
    vec->owns_data = true;

    size_t bytes = trit_packed_bytes(length, pack_mode);
    vec->packed_bytes = bytes;

    if (pack_mode == TERNARY_PACK_NONE) {
        vec->data.unpacked = (trit_t*)nimcp_calloc(length, sizeof(trit_t));
        if (!vec->data.unpacked) {
            nimcp_free(vec);
            return NULL;
        }
    } else {
        vec->data.packed = (uint8_t*)nimcp_calloc(bytes, 1);
        if (!vec->data.packed) {
            nimcp_free(vec);
            return NULL;
        }
    }

    return vec;
}

/**
 * @brief Create vector initialized to constant value
 *
 * @param length Number of trits
 * @param value Initial value for all elements
 * @param pack_mode Storage packing mode
 * @return Pointer to vector, or NULL on failure
 */
static inline trit_vector_t* trit_vector_create_filled(size_t length, trit_t value,
                                                        ternary_pack_mode_t pack_mode) {
    trit_vector_t* vec = trit_vector_create(length, pack_mode);
    if (!vec) return NULL;

    if (pack_mode == TERNARY_PACK_NONE) {
        for (size_t i = 0; i < length; i++) {
            vec->data.unpacked[i] = value;
        }
    } else if (pack_mode == TERNARY_PACK_2BIT) {
        trit_t trits[4] = {value, value, value, value};
        trit_packed2_t packed = trit_pack4(trits);
        memset(vec->data.packed, packed, vec->packed_bytes);
    } else {
        trit_t trits[5] = {value, value, value, value, value};
        trit_packed243_t packed = trit_pack5(trits);
        memset(vec->data.packed, packed, vec->packed_bytes);
    }

    return vec;
}

/**
 * @brief Destroy a ternary vector
 *
 * @param vec Vector to destroy
 */
static inline void trit_vector_destroy(trit_vector_t* vec) {
    if (!vec) return;
    if (vec->magic != TERNARY_MAGIC) return;

    if (vec->owns_data) {
        if (vec->pack_mode == TERNARY_PACK_NONE) {
            nimcp_free(vec->data.unpacked);
        } else {
            nimcp_free(vec->data.packed);
        }
    }

    vec->magic = 0;
    nimcp_free(vec);
}

/**
 * @brief Clone a ternary vector
 *
 * @param src Source vector
 * @return New vector copy, or NULL on failure
 */
static inline trit_vector_t* trit_vector_clone(const trit_vector_t* src) {
    if (!src || src->magic != TERNARY_MAGIC) return NULL;

    trit_vector_t* dst = trit_vector_create(src->length, src->pack_mode);
    if (!dst) return NULL;

    if (src->pack_mode == TERNARY_PACK_NONE) {
        memcpy(dst->data.unpacked, src->data.unpacked, src->length * sizeof(trit_t));
    } else {
        memcpy(dst->data.packed, src->data.packed, src->packed_bytes);
    }

    return dst;
}

//=============================================================================
// Element Access
//=============================================================================

/**
 * @brief Get element at index
 *
 * @param vec Vector
 * @param index Element index
 * @return Trit value, or TRIT_UNKNOWN if invalid
 */
static inline trit_t trit_vector_get(const trit_vector_t* vec, size_t index) {
    if (!vec || vec->magic != TERNARY_MAGIC || index >= vec->length) {
        return TRIT_UNKNOWN;
    }

    if (vec->pack_mode == TERNARY_PACK_NONE) {
        return vec->data.unpacked[index];
    } else if (vec->pack_mode == TERNARY_PACK_2BIT) {
        trit_t trits[4];
        trit_unpack4(vec->data.packed[index / 4], trits);
        return trits[index % 4];
    } else {
        trit_t trits[5];
        trit_unpack5(vec->data.packed[index / 5], trits);
        return trits[index % 5];
    }
}

/**
 * @brief Set element at index
 *
 * @param vec Vector
 * @param index Element index
 * @param value New value
 * @return TERNARY_OK on success
 */
static inline ternary_error_t trit_vector_set(trit_vector_t* vec, size_t index, trit_t value) {
    if (!vec || vec->magic != TERNARY_MAGIC) return TERNARY_ERR_NULL;
    if (index >= vec->length) return TERNARY_ERR_INDEX;
    if (!TRIT_IS_VALID(value)) return TERNARY_ERR_INVALID;

    if (vec->pack_mode == TERNARY_PACK_NONE) {
        vec->data.unpacked[index] = value;
    } else if (vec->pack_mode == TERNARY_PACK_2BIT) {
        trit_t trits[4];
        size_t byte_idx = index / 4;
        trit_unpack4(vec->data.packed[byte_idx], trits);
        trits[index % 4] = value;
        vec->data.packed[byte_idx] = trit_pack4(trits);
    } else {
        trit_t trits[5];
        size_t byte_idx = index / 5;
        trit_unpack5(vec->data.packed[byte_idx], trits);
        trits[index % 5] = value;
        vec->data.packed[byte_idx] = trit_pack5(trits);
    }

    return TERNARY_OK;
}

//=============================================================================
// Vector Arithmetic
//=============================================================================

/**
 * @brief Element-wise AND of two vectors
 *
 * @param a First vector
 * @param b Second vector
 * @return Result vector, or NULL on failure
 */
static inline trit_vector_t* trit_vector_and(const trit_vector_t* a, const trit_vector_t* b) {
    if (!a || !b || a->magic != TERNARY_MAGIC || b->magic != TERNARY_MAGIC) return NULL;
    if (a->length != b->length) return NULL;

    trit_vector_t* result = trit_vector_create(a->length, TERNARY_PACK_NONE);
    if (!result) return NULL;

    for (size_t i = 0; i < a->length; i++) {
        trit_t va = trit_vector_get(a, i);
        trit_t vb = trit_vector_get(b, i);
        result->data.unpacked[i] = trit_and(va, vb);
    }

    return result;
}

/**
 * @brief Element-wise OR of two vectors
 *
 * @param a First vector
 * @param b Second vector
 * @return Result vector, or NULL on failure
 */
static inline trit_vector_t* trit_vector_or(const trit_vector_t* a, const trit_vector_t* b) {
    if (!a || !b || a->magic != TERNARY_MAGIC || b->magic != TERNARY_MAGIC) return NULL;
    if (a->length != b->length) return NULL;

    trit_vector_t* result = trit_vector_create(a->length, TERNARY_PACK_NONE);
    if (!result) return NULL;

    for (size_t i = 0; i < a->length; i++) {
        trit_t va = trit_vector_get(a, i);
        trit_t vb = trit_vector_get(b, i);
        result->data.unpacked[i] = trit_or(va, vb);
    }

    return result;
}

/**
 * @brief Element-wise NOT of a vector
 *
 * @param vec Input vector
 * @return Result vector, or NULL on failure
 */
static inline trit_vector_t* trit_vector_not(const trit_vector_t* vec) {
    if (!vec || vec->magic != TERNARY_MAGIC) return NULL;

    trit_vector_t* result = trit_vector_create(vec->length, TERNARY_PACK_NONE);
    if (!result) return NULL;

    for (size_t i = 0; i < vec->length; i++) {
        result->data.unpacked[i] = trit_not(trit_vector_get(vec, i));
    }

    return result;
}

/**
 * @brief Element-wise addition (saturating)
 *
 * @param a First vector
 * @param b Second vector
 * @return Result vector (clamped to [-1,+1]), or NULL on failure
 */
static inline trit_vector_t* trit_vector_add(const trit_vector_t* a, const trit_vector_t* b) {
    if (!a || !b || a->magic != TERNARY_MAGIC || b->magic != TERNARY_MAGIC) return NULL;
    if (a->length != b->length) return NULL;

    trit_vector_t* result = trit_vector_create(a->length, TERNARY_PACK_NONE);
    if (!result) return NULL;

    for (size_t i = 0; i < a->length; i++) {
        int sum = trit_vector_get(a, i) + trit_vector_get(b, i);
        result->data.unpacked[i] = TRIT_CLAMP(sum);
    }

    return result;
}

/**
 * @brief Element-wise multiplication
 *
 * @param a First vector
 * @param b Second vector
 * @return Result vector, or NULL on failure
 */
static inline trit_vector_t* trit_vector_mul(const trit_vector_t* a, const trit_vector_t* b) {
    if (!a || !b || a->magic != TERNARY_MAGIC || b->magic != TERNARY_MAGIC) return NULL;
    if (a->length != b->length) return NULL;

    trit_vector_t* result = trit_vector_create(a->length, TERNARY_PACK_NONE);
    if (!result) return NULL;

    for (size_t i = 0; i < a->length; i++) {
        trit_t va = trit_vector_get(a, i);
        trit_t vb = trit_vector_get(b, i);
        result->data.unpacked[i] = (trit_t)(va * vb);  /* -1*-1=1, -1*0=0, etc */
    }

    return result;
}

//=============================================================================
// Aggregation Operations
//=============================================================================

/**
 * @brief Compute majority vote of vector elements
 *
 * @param vec Input vector
 * @return Majority trit value
 */
static inline trit_t trit_vector_majority(const trit_vector_t* vec) {
    if (!vec || vec->magic != TERNARY_MAGIC || vec->length == 0) {
        return TRIT_UNKNOWN;
    }

    int sum = 0;
    for (size_t i = 0; i < vec->length; i++) {
        sum += trit_vector_get(vec, i);
    }

    return TRIT_SIGN(sum);
}

/**
 * @brief Check if all elements are POSITIVE
 *
 * @param vec Input vector
 * @return true if all elements are +1
 */
static inline bool trit_vector_all_positive(const trit_vector_t* vec) {
    if (!vec || vec->magic != TERNARY_MAGIC) return false;

    for (size_t i = 0; i < vec->length; i++) {
        if (trit_vector_get(vec, i) != TRIT_POSITIVE) return false;
    }
    return true;
}

/**
 * @brief Check if any element is POSITIVE
 *
 * @param vec Input vector
 * @return true if at least one element is +1
 */
static inline bool trit_vector_any_positive(const trit_vector_t* vec) {
    if (!vec || vec->magic != TERNARY_MAGIC) return false;

    for (size_t i = 0; i < vec->length; i++) {
        if (trit_vector_get(vec, i) == TRIT_POSITIVE) return true;
    }
    return false;
}

/**
 * @brief Check if any element is NEGATIVE
 *
 * @param vec Input vector
 * @return true if at least one element is -1
 */
static inline bool trit_vector_any_negative(const trit_vector_t* vec) {
    if (!vec || vec->magic != TERNARY_MAGIC) return false;

    for (size_t i = 0; i < vec->length; i++) {
        if (trit_vector_get(vec, i) == TRIT_NEGATIVE) return true;
    }
    return false;
}

/**
 * @brief Count elements by value
 *
 * @param vec Input vector
 * @param n_positive Output count of +1 values
 * @param n_unknown Output count of 0 values
 * @param n_negative Output count of -1 values
 */
static inline void trit_vector_count(const trit_vector_t* vec,
                                      size_t* n_positive,
                                      size_t* n_unknown,
                                      size_t* n_negative) {
    size_t pos = 0, unk = 0, neg = 0;

    if (vec && vec->magic == TERNARY_MAGIC) {
        for (size_t i = 0; i < vec->length; i++) {
            trit_t t = trit_vector_get(vec, i);
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
 * @brief Get memory size of a ternary vector in bytes
 *
 * @param vec Input vector
 * @return Memory size in bytes (data + overhead)
 */
static inline size_t trit_vector_memory_size(const trit_vector_t* vec) {
    if (!vec || vec->magic != TERNARY_MAGIC) return 0;

    size_t data_size;
    if (vec->pack_mode == TERNARY_PACK_NONE) {
        data_size = vec->length * sizeof(trit_t);
    } else if (vec->pack_mode == TERNARY_PACK_2BIT) {
        data_size = (vec->length + 3) / 4;  // 4 trits per byte
    } else {
        data_size = (vec->length + 4) / 5;  // 5 trits per byte
    }

    return sizeof(trit_vector_t) + data_size;
}

/**
 * @brief Compute dot product of two vectors
 *
 * @param a First vector
 * @param b Second vector
 * @return Integer dot product
 */
static inline int trit_vector_dot(const trit_vector_t* a, const trit_vector_t* b) {
    if (!a || !b || a->magic != TERNARY_MAGIC || b->magic != TERNARY_MAGIC) return 0;
    if (a->length != b->length) return 0;

    int sum = 0;
    for (size_t i = 0; i < a->length; i++) {
        sum += trit_vector_get(a, i) * trit_vector_get(b, i);
    }
    return sum;
}

/**
 * @brief Compute Hamming distance between two vectors
 *
 * @param a First vector
 * @param b Second vector
 * @return Number of differing positions
 */
static inline size_t trit_vector_hamming(const trit_vector_t* a, const trit_vector_t* b) {
    if (!a || !b || a->magic != TERNARY_MAGIC || b->magic != TERNARY_MAGIC) return 0;
    if (a->length != b->length) return 0;

    size_t dist = 0;
    for (size_t i = 0; i < a->length; i++) {
        if (trit_vector_get(a, i) != trit_vector_get(b, i)) dist++;
    }
    return dist;
}

//=============================================================================
// Pack Mode Conversion
//=============================================================================

/**
 * @brief Convert vector to different pack mode
 *
 * @param vec Source vector
 * @param new_mode Target packing mode
 * @return New vector in specified mode, or NULL on failure
 */
static inline trit_vector_t* trit_vector_convert(const trit_vector_t* vec,
                                                  ternary_pack_mode_t new_mode) {
    if (!vec || vec->magic != TERNARY_MAGIC) return NULL;

    trit_vector_t* result = trit_vector_create(vec->length, new_mode);
    if (!result) return NULL;

    for (size_t i = 0; i < vec->length; i++) {
        trit_vector_set(result, i, trit_vector_get(vec, i));
    }

    return result;
}

/**
 * @brief Get raw pointer to unpacked data (for fast access)
 *
 * @param vec Vector (must be unpacked mode)
 * @return Pointer to data, or NULL if packed
 */
static inline trit_t* trit_vector_data(trit_vector_t* vec) {
    if (!vec || vec->magic != TERNARY_MAGIC) return NULL;
    if (vec->pack_mode != TERNARY_PACK_NONE) return NULL;
    return vec->data.unpacked;
}

/**
 * @brief Get const raw pointer to unpacked data
 *
 * @param vec Vector (must be unpacked mode)
 * @return Const pointer to data, or NULL if packed
 */
static inline const trit_t* trit_vector_data_const(const trit_vector_t* vec) {
    if (!vec || vec->magic != TERNARY_MAGIC) return NULL;
    if (vec->pack_mode != TERNARY_PACK_NONE) return NULL;
    return vec->data.unpacked;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TERNARY_VECTOR_H */
