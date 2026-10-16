//=============================================================================
// nimcp_ternary_convert.h - Ternary Type Conversion Utilities
//=============================================================================
/**
 * @file nimcp_ternary_convert.h
 * @brief Conversion utilities between ternary and other numeric types
 *
 * WHAT: Functions to convert between ternary and float/int/binary
 * WHY:  Bridge ternary logic with continuous neural computations
 * HOW:  Threshold-based quantization and soft expansion
 *
 * BIOLOGICAL BASIS:
 * - Synaptic weight quantization from continuous learning
 * - Thresholded activation (subthreshold→0, supra→±1)
 * - Precision reduction in long-range connections
 *
 * CONVERSION STRATEGIES:
 * 1. Sign-based: negative→-1, zero→0, positive→+1
 * 2. Threshold-based: |x| < τ → 0, x ≥ τ → +1, x ≤ -τ → -1
 * 3. Probabilistic: sample from softmax over {-1, 0, +1}
 *
 * @author NIMCP Development Team
 * @date 2025-12-21
 */

#ifndef NIMCP_TERNARY_CONVERT_H
#define NIMCP_TERNARY_CONVERT_H

#include "nimcp_ternary_types.h"
#include "nimcp_ternary_vector.h"
#include "nimcp_ternary_matrix.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Scalar Conversion
//=============================================================================

/**
 * @brief Convert float to trit using sign
 *
 * WHAT: Simple sign-based quantization
 * WHY:  Fastest conversion, preserves direction
 * HOW:  negative→-1, zero→0, positive→+1
 *
 * @param x Input float
 * @return Trit value
 */
static inline trit_t trit_from_float_sign(float x) {
    if (x > 0.0f) return TRIT_POSITIVE;
    if (x < 0.0f) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Convert float to trit using threshold
 *
 * WHAT: Threshold-based quantization with dead zone
 * WHY:  Ignore small values, only strong signals become ±1
 * HOW:  |x| < threshold → 0
 *
 * @param x Input float
 * @param threshold Dead zone threshold (positive)
 * @return Trit value
 */
static inline trit_t trit_from_float_threshold(float x, float threshold) {
    if (x >= threshold) return TRIT_POSITIVE;
    if (x <= -threshold) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Convert float to trit using tanh-like soft threshold
 *
 * WHAT: Smooth quantization with configurable steepness
 * WHY:  Gradual transition between states
 * HOW:  Apply tanh then threshold
 *
 * @param x Input float
 * @param steepness Steepness of transition
 * @param threshold Threshold after tanh
 * @return Trit value
 */
static inline trit_t trit_from_float_soft(float x, float steepness, float threshold) {
    float y = tanhf(steepness * x);
    if (y >= threshold) return TRIT_POSITIVE;
    if (y <= -threshold) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Convert trit to float
 *
 * WHAT: Simple expansion to float
 * WHY:  Interface with continuous computations
 * HOW:  -1→-1.0, 0→0.0, +1→+1.0
 *
 * @param t Input trit
 * @return Float value
 */
static inline float trit_to_float(trit_t t) {
    return (float)t;
}

/**
 * @brief Convert trit to scaled float
 *
 * WHAT: Expansion with scaling factor
 * WHY:  Map ternary to arbitrary range
 * HOW:  Multiply by scale
 *
 * @param t Input trit
 * @param scale Scaling factor
 * @return Scaled float value
 */
static inline float trit_to_float_scaled(trit_t t, float scale) {
    return (float)t * scale;
}

/**
 * @brief Convert binary to trit
 *
 * WHAT: Map boolean to ternary
 * WHY:  Interface with binary systems
 * HOW:  false→-1, true→+1
 *
 * @param b Boolean value
 * @return Trit value
 */
static inline trit_t trit_from_bool(bool b) {
    return b ? TRIT_POSITIVE : TRIT_NEGATIVE;
}

/**
 * @brief Convert binary to trit (with unknown option)
 *
 * WHAT: Map int to ternary with third state
 * WHY:  Represent missing/undefined in binary source
 * HOW:  negative→-1, 0→unknown, positive→+1
 *
 * @param val Integer value
 * @return Trit value
 */
static inline trit_t trit_from_int_tristate(int val) {
    if (val > 0) return TRIT_POSITIVE;
    if (val < 0) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

//=============================================================================
// Extended Trit Conversion
//=============================================================================

/**
 * @brief Convert float to extended trit with confidence
 *
 * WHAT: Quantize with confidence from magnitude
 * WHY:  Preserve certainty information
 * HOW:  Larger magnitude = higher confidence
 *
 * @param x Input float
 * @param threshold Dead zone threshold
 * @return Extended trit with confidence
 */
static inline trit_extended_t trit_extended_from_float(float x, float threshold) {
    trit_extended_t ext;
    float abs_x = fabsf(x);

    if (abs_x < threshold) {
        ext.value = TRIT_UNKNOWN;
        ext.confidence = 1.0f - (abs_x / threshold);
    } else {
        ext.value = (x > 0) ? TRIT_POSITIVE : TRIT_NEGATIVE;
        /* Confidence increases with distance from threshold */
        ext.confidence = fminf(1.0f, abs_x / (2.0f * threshold));
    }

    ext.uncertainty = 1.0f - ext.confidence;
    ext.inference_depth = 0;

    return ext;
}

/**
 * @brief Convert extended trit to float (using confidence)
 *
 * WHAT: Expand to float weighted by confidence
 * WHY:  Uncertain values contribute less
 * HOW:  result = value * confidence
 *
 * @param ext Extended trit
 * @return Weighted float value
 */
static inline float trit_extended_to_float(const trit_extended_t* ext) {
    if (!ext) return 0.0f;
    return (float)ext->value * ext->confidence;
}

//=============================================================================
// Vector Conversion
//=============================================================================

/**
 * @brief Convert float array to trit vector
 *
 * @param input Input float array
 * @param length Array length
 * @param threshold Quantization threshold
 * @param pack_mode Output packing mode
 * @return Trit vector, or NULL on failure
 */
static inline trit_vector_t* trit_vector_from_floats(const float* input, size_t length,
                                                      float threshold,
                                                      ternary_pack_mode_t pack_mode) {
    if (!input || length == 0) return NULL;

    trit_vector_t* vec = trit_vector_create(length, pack_mode);
    if (!vec) return NULL;

    for (size_t i = 0; i < length; i++) {
        trit_t t = trit_from_float_threshold(input[i], threshold);
        trit_vector_set(vec, i, t);
    }

    return vec;
}

/**
 * @brief Convert trit vector to float array
 *
 * @param vec Input trit vector
 * @param output Output float array (caller allocated)
 * @param scale Optional scale factor (1.0 for identity)
 * @return TERNARY_OK on success
 */
static inline ternary_error_t trit_vector_to_floats(const trit_vector_t* vec,
                                                     float* output, float scale) {
    if (!vec || vec->magic != TERNARY_MAGIC || !output) return TERNARY_ERR_NULL;

    for (size_t i = 0; i < vec->length; i++) {
        output[i] = trit_to_float_scaled(trit_vector_get(vec, i), scale);
    }

    return TERNARY_OK;
}

/**
 * @brief Convert double array to trit vector
 *
 * @param input Input double array
 * @param length Array length
 * @param threshold Quantization threshold
 * @param pack_mode Output packing mode
 * @return Trit vector, or NULL on failure
 */
static inline trit_vector_t* trit_vector_from_doubles(const double* input, size_t length,
                                                       double threshold,
                                                       ternary_pack_mode_t pack_mode) {
    if (!input || length == 0) return NULL;

    trit_vector_t* vec = trit_vector_create(length, pack_mode);
    if (!vec) return NULL;

    for (size_t i = 0; i < length; i++) {
        trit_t t;
        if (input[i] >= threshold) t = TRIT_POSITIVE;
        else if (input[i] <= -threshold) t = TRIT_NEGATIVE;
        else t = TRIT_UNKNOWN;
        trit_vector_set(vec, i, t);
    }

    return vec;
}

/**
 * @brief Convert trit vector to double array
 *
 * @param vec Input trit vector
 * @param output Output double array (caller allocated)
 * @param scale Optional scale factor (1.0 for identity)
 * @return TERNARY_OK on success
 */
static inline ternary_error_t trit_vector_to_doubles(const trit_vector_t* vec,
                                                      double* output, double scale) {
    if (!vec || vec->magic != TERNARY_MAGIC || !output) return TERNARY_ERR_NULL;

    for (size_t i = 0; i < vec->length; i++) {
        output[i] = (double)trit_vector_get(vec, i) * scale;
    }

    return TERNARY_OK;
}

//=============================================================================
// Matrix Conversion
//=============================================================================

/**
 * @brief Convert float matrix to trit matrix
 *
 * @param input Input float array (row-major)
 * @param rows Number of rows
 * @param cols Number of columns
 * @param threshold Quantization threshold
 * @param pack_mode Output packing mode
 * @return Trit matrix, or NULL on failure
 */
static inline trit_matrix_t* trit_matrix_from_floats(const float* input,
                                                      size_t rows, size_t cols,
                                                      float threshold,
                                                      ternary_pack_mode_t pack_mode) {
    if (!input || rows == 0 || cols == 0) return NULL;

    trit_matrix_t* mat = trit_matrix_create(rows, cols, pack_mode);
    if (!mat) return NULL;

    for (size_t i = 0; i < rows * cols; i++) {
        trit_t t = trit_from_float_threshold(input[i], threshold);
        trit_matrix_set(mat, i / cols, i % cols, t);
    }

    return mat;
}

/**
 * @brief Convert trit matrix to float array
 *
 * @param mat Input trit matrix
 * @param output Output float array (caller allocated, row-major)
 * @param scale Optional scale factor (1.0 for identity)
 * @return TERNARY_OK on success
 */
static inline ternary_error_t trit_matrix_to_floats(const trit_matrix_t* mat,
                                                     float* output, float scale) {
    if (!mat || mat->magic != TERNARY_MAGIC || !output) return TERNARY_ERR_NULL;

    for (size_t row = 0; row < mat->rows; row++) {
        for (size_t col = 0; col < mat->cols; col++) {
            output[row * mat->cols + col] =
                trit_to_float_scaled(trit_matrix_get(mat, row, col), scale);
        }
    }

    return TERNARY_OK;
}

//=============================================================================
// Probabilistic Conversion
//=============================================================================

/**
 * @brief Stochastic quantization of float to trit
 *
 * WHAT: Probabilistic rounding based on value
 * WHY:  Preserve gradient information in expectation
 * HOW:  Sample proportional to distance from quantization levels
 *
 * Uses simple linear interpolation probability:
 * P(+1) = max(0, x) for x in [-1, 1]
 * P(-1) = max(0, -x) for x in [-1, 1]
 * P(0) = 1 - P(+1) - P(-1)
 *
 * @param x Input float (expected in [-1, 1] range)
 * @param rand_val Random value in [0, 1)
 * @return Stochastically sampled trit
 */
static inline trit_t trit_from_float_stochastic(float x, float rand_val) {
    /* Clamp to reasonable range */
    float clamped = fmaxf(-1.0f, fminf(1.0f, x));

    float p_pos = fmaxf(0.0f, clamped);
    float p_neg = fmaxf(0.0f, -clamped);

    if (rand_val < p_neg) return TRIT_NEGATIVE;
    if (rand_val < p_neg + p_pos) return TRIT_POSITIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Convert softmax probabilities to trit
 *
 * WHAT: Sample from categorical distribution over trits
 * WHY:  Use learned probabilities for quantization
 * HOW:  Probabilities for {-1, 0, +1}
 *
 * @param p_negative Probability of -1
 * @param p_unknown Probability of 0
 * @param p_positive Probability of +1
 * @param rand_val Random value in [0, 1)
 * @return Sampled trit
 */
static inline trit_t trit_from_probabilities(float p_negative, float p_unknown,
                                              float p_positive, float rand_val) {
    if (rand_val < p_negative) return TRIT_NEGATIVE;
    if (rand_val < p_negative + p_unknown) return TRIT_UNKNOWN;
    return TRIT_POSITIVE;
}

/**
 * @brief Get probability distribution for trit (one-hot soft)
 *
 * WHAT: Convert trit to probability distribution
 * WHY:  Soft targets for training
 * HOW:  Mostly one-hot with small label smoothing
 *
 * @param t Input trit
 * @param smoothing Label smoothing factor [0, 1)
 * @param p_negative Output probability of -1
 * @param p_unknown Output probability of 0
 * @param p_positive Output probability of +1
 */
static inline void trit_to_probabilities(trit_t t, float smoothing,
                                          float* p_negative, float* p_unknown,
                                          float* p_positive) {
    float base = smoothing / 3.0f;
    float peak = 1.0f - smoothing + base;

    *p_negative = (t == TRIT_NEGATIVE) ? peak : base;
    *p_unknown = (t == TRIT_UNKNOWN) ? peak : base;
    *p_positive = (t == TRIT_POSITIVE) ? peak : base;
}

//=============================================================================
// Neural Weight Conversion
//=============================================================================

/**
 * @brief Quantize continuous weight to ternary
 *
 * WHAT: Convert float weight to ternary with adaptive threshold
 * WHY:  SNN ternary weight compression
 * HOW:  Threshold based on weight distribution percentile
 *
 * @param weight Input weight
 * @param threshold_scale Scale factor for threshold
 * @param weight_mean Mean of weight distribution
 * @param weight_std Standard deviation of weight distribution
 * @return Ternary weight
 */
static inline trit_t trit_quantize_weight(float weight, float threshold_scale,
                                           float weight_mean, float weight_std) {
    /* Normalized threshold: weights within threshold_scale * std are zero */
    float threshold = threshold_scale * weight_std;
    float centered = weight - weight_mean;

    if (centered >= threshold) return TRIT_POSITIVE;
    if (centered <= -threshold) return TRIT_NEGATIVE;
    return TRIT_UNKNOWN;
}

/**
 * @brief Dequantize ternary weight to continuous
 *
 * WHAT: Expand ternary weight back to continuous
 * WHY:  Interface with float computations
 * HOW:  Map to learned or fixed scale values
 *
 * @param weight Ternary weight
 * @param positive_scale Value for +1 weights
 * @param negative_scale Value for -1 weights
 * @return Continuous weight value
 */
static inline float trit_dequantize_weight(trit_t weight, float positive_scale,
                                            float negative_scale) {
    if (weight == TRIT_POSITIVE) return positive_scale;
    if (weight == TRIT_NEGATIVE) return negative_scale;
    return 0.0f;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TERNARY_CONVERT_H */
