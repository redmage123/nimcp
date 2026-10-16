//=============================================================================
// nimcp_ternary_types.h - Core Ternary Type Definitions
//=============================================================================
/**
 * @file nimcp_ternary_types.h
 * @brief Core ternary (three-valued) type definitions for NIMCP
 *
 * WHAT: Fundamental ternary types for three-valued logic operations
 * WHY:  Enable uncertainty reasoning, fuzzy logic, and tri-state decisions
 * HOW:  Defines trit type with values {-1, 0, +1} and packed storage
 *
 * BIOLOGICAL BASIS:
 * - Maps to neural activation states: inhibitory (-1), subthreshold (0), excitatory (+1)
 * - Aligns with synaptic weight signs and uncertain/unknown states
 * - Supports biological uncertainty in perception and decision-making
 *
 * MATHEMATICAL FOUNDATIONS:
 * - Kleene strong three-valued logic (K3)
 * - Lukasiewicz infinite-valued logic (restricted to 3 values)
 * - Bochvar internal three-valued logic
 *
 * MEMORY EFFICIENCY:
 * - Unpacked: 1 byte per trit (fastest access)
 * - 2-bit packed: 4 trits per byte (fast, 75% savings)
 * - Base-243 packed: 5 trits per byte (80% savings)
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_TERNARY_TYPES_H
#define NIMCP_TERNARY_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Maximum ternary tensor dimensions */
#define TERNARY_MAX_DIMS 8

/** Default memory alignment for ternary arrays */
#define TERNARY_ALIGN 64

/** Magic number for validation */
#define TERNARY_MAGIC 0x54524954U  /* "TRIT" in ASCII */

/** Packed storage constants */
#define TRITS_PER_BYTE_PACKED5 5    /**< 5 trits/byte using base-243 */
#define TRITS_PER_BYTE_PACKED2 4    /**< 4 trits/byte using 2-bit encoding */
#define BITS_PER_TRIT_2BIT 2        /**< 2 bits per trit in 2-bit mode */

/** Base-243 encoding: 3^5 = 243 fits in 1 byte */
#define TERNARY_BASE_243 243

//=============================================================================
// Error Codes
//=============================================================================

/**
 * @brief Ternary module error codes
 */
typedef enum {
    TERNARY_OK = 0,              /**< Success */
    TERNARY_ERR_NULL = -1,       /**< Null pointer argument */
    TERNARY_ERR_SHAPE = -2,      /**< Shape mismatch */
    TERNARY_ERR_ALLOC = -3,      /**< Memory allocation failed */
    TERNARY_ERR_INDEX = -4,      /**< Index out of bounds */
    TERNARY_ERR_INVALID = -5,    /**< Invalid trit value */
    TERNARY_ERR_OVERFLOW = -6,   /**< Arithmetic overflow */
    TERNARY_ERR_CONVERSION = -7, /**< Conversion error */
    TERNARY_ERR_PACK_MODE = -8   /**< Invalid pack mode */
} ternary_error_t;

//=============================================================================
// Core Trit Type
//=============================================================================

/**
 * @brief Single ternary value (trit)
 *
 * WHAT: Three-valued logic element with values {-1, 0, +1}
 * WHY:  Enable three-state logic beyond binary true/false
 * HOW:  Signed 8-bit integer, valid values are -1, 0, +1 only
 *
 * VALUE SEMANTICS:
 * - TRIT_NEGATIVE (-1): False, inhibitory, reject, forbid
 * - TRIT_UNKNOWN   (0): Unknown, uncertain, abstain, neutral
 * - TRIT_POSITIVE (+1): True, excitatory, accept, allow
 *
 * MEMORY: 1 byte unpacked (for speed), can pack to 2 bits or 1.6 bits
 */
typedef int8_t trit_t;

/** Ternary value constants - primary names */
#define TRIT_NEGATIVE ((trit_t)-1)
#define TRIT_UNKNOWN  ((trit_t)0)
#define TRIT_POSITIVE ((trit_t)+1)

/** Alternative names for logic context */
#define TRIT_FALSE    TRIT_NEGATIVE
#define TRIT_TRUE     TRIT_POSITIVE

/** Alternative names for neural context */
#define TRIT_INHIBITORY TRIT_NEGATIVE
#define TRIT_SILENT     TRIT_UNKNOWN
#define TRIT_EXCITATORY TRIT_POSITIVE

/** Alternative names for ethics context */
#define TRIT_FORBID   TRIT_NEGATIVE
#define TRIT_NEUTRAL  TRIT_UNKNOWN
#define TRIT_ALLOW    TRIT_POSITIVE

/** Alternative names for voting context */
#define TRIT_DISAGREE TRIT_NEGATIVE
#define TRIT_ABSTAIN  TRIT_UNKNOWN
#define TRIT_AGREE    TRIT_POSITIVE

/** Alternative names for plasticity context */
#define TRIT_LTD      TRIT_NEGATIVE
#define TRIT_STABLE   TRIT_UNKNOWN
#define TRIT_LTP      TRIT_POSITIVE

//=============================================================================
// Packed Storage Types
//=============================================================================

/**
 * @brief Packed storage mode
 *
 * WHAT: Encoding scheme for trit arrays
 * WHY:  Trade-off between memory efficiency and access speed
 * HOW:  Different bit layouts per trit
 *
 * COMPARISON:
 * | Mode    | Bits/trit | Trits/byte | Speed   | Use case           |
 * |---------|-----------|------------|---------|-------------------|
 * | NONE    | 8         | 1          | Fastest | Active computation |
 * | 2BIT    | 2         | 4          | Fast    | Balanced           |
 * | BASE243 | 1.585     | 5          | Slower  | Maximum compression|
 */
typedef enum {
    TERNARY_PACK_NONE = 0,      /**< Unpacked: 1 trit per byte - fastest */
    TERNARY_PACK_2BIT = 1,      /**< 2-bit: 4 trits per byte - fast */
    TERNARY_PACK_BASE243 = 2    /**< Base-243: 5 trits per byte - smallest */
} ternary_pack_mode_t;

/**
 * @brief Packed 2-bit encoding (4 trits per byte)
 *
 * 2-bit encoding scheme:
 * - 00 = UNKNOWN (0)
 * - 01 = POSITIVE (+1)
 * - 10 = NEGATIVE (-1)
 * - 11 = reserved (treated as UNKNOWN)
 *
 * Byte layout: [trit3:2][trit2:2][trit1:2][trit0:2]
 * Most significant bits hold higher indices.
 */
typedef uint8_t trit_packed2_t;

/**
 * @brief Packed base-243 encoding (5 trits per byte)
 *
 * 5 trits encode to single byte using base-3 arithmetic:
 * value = t0 + 3*t1 + 9*t2 + 27*t3 + 81*t4
 * where each t_i in {0,1,2} (offset from -1)
 * Range: 0 to 242 (fits in 8 bits since 3^5 = 243)
 *
 * More space-efficient but slower access (requires div/mod).
 */
typedef uint8_t trit_packed243_t;

//=============================================================================
// Validation Macros
//=============================================================================

/**
 * @brief Check if value is a valid trit
 */
#define TRIT_IS_VALID(t) ((t) >= TRIT_NEGATIVE && (t) <= TRIT_POSITIVE)

/**
 * @brief Clamp value to valid trit range
 */
#define TRIT_CLAMP(t) ((t) < TRIT_NEGATIVE ? TRIT_NEGATIVE : \
                       ((t) > TRIT_POSITIVE ? TRIT_POSITIVE : (t)))

/**
 * @brief Convert any nonzero to sign, zero stays zero
 */
#define TRIT_SIGN(x) ((x) > 0 ? TRIT_POSITIVE : ((x) < 0 ? TRIT_NEGATIVE : TRIT_UNKNOWN))

//=============================================================================
// Shape Descriptor
//=============================================================================

/**
 * @brief Ternary array shape descriptor
 *
 * WHAT: Describes dimensions and memory layout of ternary arrays
 * WHY:  Enable N-dimensional ternary tensors
 * HOW:  Following nimcp_tensor pattern
 */
typedef struct {
    uint32_t rank;                          /**< Number of dimensions (0 = scalar) */
    uint32_t dims[TERNARY_MAX_DIMS];        /**< Size of each dimension */
    int64_t strides[TERNARY_MAX_DIMS];      /**< Stride for each dimension (in trits) */
    size_t numel;                           /**< Total number of trits */
    size_t packed_bytes;                    /**< Bytes needed in packed storage */
    ternary_pack_mode_t pack_mode;          /**< Current packing mode */
} ternary_shape_t;

//=============================================================================
// Extended Trit with Metadata
//=============================================================================

/**
 * @brief Extended trit value with confidence and provenance
 *
 * WHAT: Trit with additional metadata for reasoning
 * WHY:  Track confidence and source of ternary values in inference
 * HOW:  Combines discrete trit with continuous confidence
 *
 * BIOLOGICAL BASIS:
 * - Prefrontal cortex maintains uncertainty representations
 * - Dopamine signals confidence in predictions
 */
typedef struct {
    trit_t value;                  /**< Ternary truth value */
    float confidence;              /**< Confidence in this value [0.0-1.0] */
    float uncertainty;             /**< Epistemic uncertainty [0.0-1.0] */
    uint32_t inference_depth;      /**< Inference steps to derive this */
} trit_extended_t;

//=============================================================================
// Utility Functions (Inline)
//=============================================================================

/**
 * @brief Validate trit value
 *
 * @param t Value to check
 * @return true if valid trit (-1, 0, or +1)
 */
static inline bool trit_is_valid(trit_t t) {
    return t >= TRIT_NEGATIVE && t <= TRIT_POSITIVE;
}

/**
 * @brief Normalize any integer to valid trit (sign function)
 *
 * @param val Any integer value
 * @return Trit: -1 if negative, 0 if zero, +1 if positive
 */
static inline trit_t trit_from_int(int val) {
    if (val > 0) return TRIT_POSITIVE;
    if (val < 0) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Create extended trit with full confidence
 *
 * @param value Trit value
 * @return Extended trit with confidence 1.0
 */
static inline trit_extended_t trit_extended_certain(trit_t value) {
    trit_extended_t ext = {
        .value = value,
        .confidence = 1.0f,
        .uncertainty = 0.0f,
        .inference_depth = 0
    };
    return ext;
}

/**
 * @brief Create extended trit with specified confidence
 *
 * @param value Trit value
 * @param confidence Confidence level [0.0-1.0]
 * @return Extended trit
 */
static inline trit_extended_t trit_extended_create(trit_t value, float confidence) {
    trit_extended_t ext = {
        .value = value,
        .confidence = confidence,
        .uncertainty = 1.0f - confidence,
        .inference_depth = 0
    };
    return ext;
}

/**
 * @brief Calculate bytes needed for packed storage
 *
 * @param numel Number of trits
 * @param mode Packing mode
 * @return Bytes required
 */
static inline size_t trit_packed_bytes(size_t numel, ternary_pack_mode_t mode) {
    switch (mode) {
        case TERNARY_PACK_NONE:
            return numel;
        case TERNARY_PACK_2BIT:
            return (numel + 3) / 4;  /* Ceiling division by 4 */
        case TERNARY_PACK_BASE243:
            return (numel + 4) / 5;  /* Ceiling division by 5 */
        default:
            return numel;
    }
}

/**
 * @brief Get string representation of trit value
 *
 * @param t Trit value
 * @return String: "-1", "0", or "+1"
 */
static inline const char* trit_to_string(trit_t t) {
    switch (t) {
        case TRIT_NEGATIVE: return "-1";
        case TRIT_UNKNOWN:  return "0";
        case TRIT_POSITIVE: return "+1";
        default:            return "?";
    }
}

/**
 * @brief Get descriptive name for trit value
 *
 * @param t Trit value
 * @return String: "NEGATIVE", "UNKNOWN", or "POSITIVE"
 */
static inline const char* trit_name(trit_t t) {
    switch (t) {
        case TRIT_NEGATIVE: return "NEGATIVE";
        case TRIT_UNKNOWN:  return "UNKNOWN";
        case TRIT_POSITIVE: return "POSITIVE";
        default:            return "INVALID";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TERNARY_TYPES_H */
