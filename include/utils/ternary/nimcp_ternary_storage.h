//=============================================================================
// nimcp_ternary_storage.h - Packed Ternary Storage Operations
//=============================================================================
/**
 * @file nimcp_ternary_storage.h
 * @brief Packed storage operations for ternary arrays
 *
 * WHAT: Encode/decode operations for packed trit storage
 * WHY:  Memory efficiency for large ternary arrays (synaptic weights, etc.)
 * HOW:  Two packing modes: 2-bit (4 trits/byte) and base-243 (5 trits/byte)
 *
 * PACKING MODES:
 *
 * 2-bit encoding (fast, 4 trits per byte):
 *   - 00 = UNKNOWN (0)
 *   - 01 = POSITIVE (+1)
 *   - 10 = NEGATIVE (-1)
 *   - 11 = reserved (treated as UNKNOWN)
 *   - Byte layout: [trit3:2][trit2:2][trit1:2][trit0:2]
 *   - 4x compression vs unpacked
 *
 * Base-243 encoding (compact, 5 trits per byte):
 *   - Each trit mapped to {0,1,2} (offset from -1)
 *   - Value = t0 + 3*t1 + 9*t2 + 27*t3 + 81*t4
 *   - Range: 0-242 fits in 8 bits (3^5 = 243)
 *   - 5x compression vs unpacked
 *   - Requires division/modulo for access
 *
 * PERFORMANCE CONSIDERATIONS:
 * - 2-bit: Use for active computation, random access
 * - Base-243: Use for storage, sequential access, maximum compression
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_TERNARY_STORAGE_H
#define NIMCP_TERNARY_STORAGE_H

#include "nimcp_ternary_types.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// 2-Bit Packed Encoding (4 trits per byte)
//=============================================================================

/**
 * 2-bit encoding lookup tables for fast encode/decode
 *
 * Encoding: {-1, 0, +1} -> {2, 0, 1}
 *   - UNKNOWN (0) -> 00
 *   - POSITIVE (+1) -> 01
 *   - NEGATIVE (-1) -> 10
 *
 * Decoding: {0, 1, 2, 3} -> {0, +1, -1, 0}
 */

/** Encode trit to 2-bit value */
#define TRIT_TO_2BIT(t) ((uint8_t)((t) == TRIT_POSITIVE ? 1 : ((t) == TRIT_NEGATIVE ? 2 : 0)))

/** Decode 2-bit value to trit */
#define TRIT_FROM_2BIT(b) ((trit_t)((b) == 1 ? TRIT_POSITIVE : ((b) == 2 ? TRIT_NEGATIVE : TRIT_UNKNOWN)))

/**
 * @brief Encode 4 trits into single byte (2-bit encoding)
 *
 * WHAT: Pack 4 trits into 8 bits using 2 bits each
 * WHY:  Fast packing with simple bitwise operations
 * HOW:  Each trit encoded as 00/01/10, packed LSB-first
 *
 * @param t Array of 4 trits (values in {-1, 0, +1})
 * @return Packed byte
 */
static inline trit_packed2_t trit_pack4(const trit_t t[4]) {
    uint8_t b0 = TRIT_TO_2BIT(t[0]);
    uint8_t b1 = TRIT_TO_2BIT(t[1]);
    uint8_t b2 = TRIT_TO_2BIT(t[2]);
    uint8_t b3 = TRIT_TO_2BIT(t[3]);
    return (trit_packed2_t)(b0 | (b1 << 2) | (b2 << 4) | (b3 << 6));
}

/**
 * @brief Decode byte into 4 trits (2-bit encoding)
 *
 * @param packed Packed byte
 * @param t Output array of 4 trits
 */
static inline void trit_unpack4(trit_packed2_t packed, trit_t t[4]) {
    t[0] = TRIT_FROM_2BIT((packed >> 0) & 0x03);
    t[1] = TRIT_FROM_2BIT((packed >> 2) & 0x03);
    t[2] = TRIT_FROM_2BIT((packed >> 4) & 0x03);
    t[3] = TRIT_FROM_2BIT((packed >> 6) & 0x03);
}

/**
 * @brief Get single trit from 2-bit packed array
 *
 * @param data Packed byte array
 * @param index Trit index (0-based)
 * @return Trit value
 */
static inline trit_t trit_get_packed2(const trit_packed2_t* data, size_t index) {
    size_t byte_idx = index / 4;
    size_t bit_offset = (index % 4) * 2;
    uint8_t bits = (data[byte_idx] >> bit_offset) & 0x03;
    return TRIT_FROM_2BIT(bits);
}

/**
 * @brief Set single trit in 2-bit packed array
 *
 * @param data Packed byte array
 * @param index Trit index (0-based)
 * @param value Trit value to set
 */
static inline void trit_set_packed2(trit_packed2_t* data, size_t index, trit_t value) {
    size_t byte_idx = index / 4;
    size_t bit_offset = (index % 4) * 2;
    uint8_t mask = ~(0x03 << bit_offset);
    uint8_t bits = TRIT_TO_2BIT(value) << bit_offset;
    data[byte_idx] = (data[byte_idx] & mask) | bits;
}

//=============================================================================
// Base-243 Packed Encoding (5 trits per byte)
//=============================================================================

/**
 * Powers of 3 for base-243 encoding
 */
static const uint8_t TRIT_POW3[5] = {1, 3, 9, 27, 81};

/**
 * @brief Encode 5 trits into single byte (base-243 encoding)
 *
 * WHAT: Pack 5 trits into 8 bits using base-3 arithmetic
 * WHY:  Maximum packing efficiency (1.6 bits/trit vs 2 bits)
 * HOW:  value = sum(t[i] * 3^i) where t[i] in {0,1,2}
 *
 * @param t Array of 5 trits (values in {-1, 0, +1})
 * @return Packed byte [0, 242]
 */
static inline trit_packed243_t trit_pack5(const trit_t t[5]) {
    /* Map {-1, 0, +1} to {0, 1, 2} then encode base-3 */
    uint8_t v0 = (uint8_t)(t[0] + 1);
    uint8_t v1 = (uint8_t)(t[1] + 1);
    uint8_t v2 = (uint8_t)(t[2] + 1);
    uint8_t v3 = (uint8_t)(t[3] + 1);
    uint8_t v4 = (uint8_t)(t[4] + 1);
    return (trit_packed243_t)(v0 + 3*v1 + 9*v2 + 27*v3 + 81*v4);
}

/**
 * @brief Decode byte into 5 trits (base-243 encoding)
 *
 * @param packed Packed byte
 * @param t Output array of 5 trits
 */
static inline void trit_unpack5(trit_packed243_t packed, trit_t t[5]) {
    uint8_t v = packed;
    t[0] = (trit_t)((v % 3) - 1); v /= 3;
    t[1] = (trit_t)((v % 3) - 1); v /= 3;
    t[2] = (trit_t)((v % 3) - 1); v /= 3;
    t[3] = (trit_t)((v % 3) - 1); v /= 3;
    t[4] = (trit_t)(v - 1);
}

/**
 * @brief Get single trit from base-243 packed array
 *
 * NOTE: Slower than 2-bit due to division/modulo
 *
 * @param data Packed byte array
 * @param index Trit index (0-based)
 * @return Trit value
 */
static inline trit_t trit_get_packed243(const trit_packed243_t* data, size_t index) {
    size_t byte_idx = index / 5;
    size_t trit_pos = index % 5;

    /* Extract by dividing and taking modulo */
    uint8_t v = data[byte_idx];
    for (size_t i = 0; i < trit_pos; i++) {
        v /= 3;
    }
    return (trit_t)((v % 3) - 1);
}

/**
 * @brief Set single trit in base-243 packed array
 *
 * NOTE: Slower than 2-bit due to unpack/repack
 *
 * @param data Packed byte array
 * @param index Trit index (0-based)
 * @param value Trit value to set
 */
static inline void trit_set_packed243(trit_packed243_t* data, size_t index, trit_t value) {
    size_t byte_idx = index / 5;
    size_t trit_pos = index % 5;

    /* Unpack, modify, repack */
    trit_t trits[5];
    trit_unpack5(data[byte_idx], trits);
    trits[trit_pos] = value;
    data[byte_idx] = trit_pack5(trits);
}

//=============================================================================
// Generic Packed Access (Mode-Independent)
//=============================================================================

/**
 * @brief Get trit from packed array (any mode)
 *
 * @param data Packed data pointer
 * @param index Trit index (0-based)
 * @param mode Packing mode
 * @return Trit value
 */
static inline trit_t trit_get_packed(const void* data, size_t index, ternary_pack_mode_t mode) {
    switch (mode) {
        case TERNARY_PACK_NONE:
            return ((const trit_t*)data)[index];
        case TERNARY_PACK_2BIT:
            return trit_get_packed2((const trit_packed2_t*)data, index);
        case TERNARY_PACK_BASE243:
            return trit_get_packed243((const trit_packed243_t*)data, index);
        default:
            return TRIT_UNKNOWN;
    }
}

/**
 * @brief Set trit in packed array (any mode)
 *
 * @param data Packed data pointer
 * @param index Trit index (0-based)
 * @param value Trit value to set
 * @param mode Packing mode
 */
static inline void trit_set_packed(void* data, size_t index, trit_t value, ternary_pack_mode_t mode) {
    switch (mode) {
        case TERNARY_PACK_NONE:
            ((trit_t*)data)[index] = value;
            break;
        case TERNARY_PACK_2BIT:
            trit_set_packed2((trit_packed2_t*)data, index, value);
            break;
        case TERNARY_PACK_BASE243:
            trit_set_packed243((trit_packed243_t*)data, index, value);
            break;
    }
}

//=============================================================================
// Bulk Operations
//=============================================================================

/**
 * @brief Pack array of trits into 2-bit format
 *
 * @param src Source trit array
 * @param dst Destination packed array (must have (count+3)/4 bytes)
 * @param count Number of trits
 */
static inline void trit_pack_array_2bit(const trit_t* src, trit_packed2_t* dst, size_t count) {
    size_t full_groups = count / 4;
    size_t remainder = count % 4;

    /* Pack full groups of 4 */
    for (size_t i = 0; i < full_groups; i++) {
        dst[i] = trit_pack4(&src[i * 4]);
    }

    /* Handle remainder */
    if (remainder > 0) {
        trit_t temp[4] = {TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_UNKNOWN};
        for (size_t i = 0; i < remainder; i++) {
            temp[i] = src[full_groups * 4 + i];
        }
        dst[full_groups] = trit_pack4(temp);
    }
}

/**
 * @brief Unpack 2-bit array into trit array
 *
 * @param src Source packed array
 * @param dst Destination trit array (must have count elements)
 * @param count Number of trits
 */
static inline void trit_unpack_array_2bit(const trit_packed2_t* src, trit_t* dst, size_t count) {
    size_t full_groups = count / 4;
    size_t remainder = count % 4;

    /* Unpack full groups of 4 */
    for (size_t i = 0; i < full_groups; i++) {
        trit_unpack4(src[i], &dst[i * 4]);
    }

    /* Handle remainder */
    if (remainder > 0) {
        trit_t temp[4];
        trit_unpack4(src[full_groups], temp);
        for (size_t i = 0; i < remainder; i++) {
            dst[full_groups * 4 + i] = temp[i];
        }
    }
}

/**
 * @brief Pack array of trits into base-243 format
 *
 * @param src Source trit array
 * @param dst Destination packed array (must have (count+4)/5 bytes)
 * @param count Number of trits
 */
static inline void trit_pack_array_243(const trit_t* src, trit_packed243_t* dst, size_t count) {
    size_t full_groups = count / 5;
    size_t remainder = count % 5;

    /* Pack full groups of 5 */
    for (size_t i = 0; i < full_groups; i++) {
        dst[i] = trit_pack5(&src[i * 5]);
    }

    /* Handle remainder */
    if (remainder > 0) {
        trit_t temp[5] = {TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_UNKNOWN, TRIT_UNKNOWN};
        for (size_t i = 0; i < remainder; i++) {
            temp[i] = src[full_groups * 5 + i];
        }
        dst[full_groups] = trit_pack5(temp);
    }
}

/**
 * @brief Unpack base-243 array into trit array
 *
 * @param src Source packed array
 * @param dst Destination trit array (must have count elements)
 * @param count Number of trits
 */
static inline void trit_unpack_array_243(const trit_packed243_t* src, trit_t* dst, size_t count) {
    size_t full_groups = count / 5;
    size_t remainder = count % 5;

    /* Unpack full groups of 5 */
    for (size_t i = 0; i < full_groups; i++) {
        trit_unpack5(src[i], &dst[i * 5]);
    }

    /* Handle remainder */
    if (remainder > 0) {
        trit_t temp[5];
        trit_unpack5(src[full_groups], temp);
        for (size_t i = 0; i < remainder; i++) {
            dst[full_groups * 5 + i] = temp[i];
        }
    }
}

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Count occurrences of each trit value in packed array
 *
 * @param data Packed data
 * @param count Number of trits
 * @param mode Packing mode
 * @param neg_count Output: count of NEGATIVE trits
 * @param unk_count Output: count of UNKNOWN trits
 * @param pos_count Output: count of POSITIVE trits
 */
static inline void trit_count_packed(
    const void* data, size_t count, ternary_pack_mode_t mode,
    size_t* neg_count, size_t* unk_count, size_t* pos_count
) {
    size_t neg = 0, unk = 0, pos = 0;

    for (size_t i = 0; i < count; i++) {
        trit_t t = trit_get_packed(data, i, mode);
        switch (t) {
            case TRIT_NEGATIVE: neg++; break;
            case TRIT_UNKNOWN:  unk++; break;
            case TRIT_POSITIVE: pos++; break;
        }
    }

    if (neg_count) *neg_count = neg;
    if (unk_count) *unk_count = unk;
    if (pos_count) *pos_count = pos;
}

/**
 * @brief Count non-zero trits in packed array (sparsity metric)
 *
 * @param data Packed data
 * @param count Number of trits
 * @param mode Packing mode
 * @return Number of non-UNKNOWN trits
 */
static inline size_t trit_count_nonzero_packed(const void* data, size_t count, ternary_pack_mode_t mode) {
    size_t nonzero = 0;
    for (size_t i = 0; i < count; i++) {
        if (trit_get_packed(data, i, mode) != TRIT_UNKNOWN) {
            nonzero++;
        }
    }
    return nonzero;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TERNARY_STORAGE_H */
