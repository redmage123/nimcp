//=============================================================================
// nimcp_lnn_numerical.h - Numerical Safety Utilities for LNN
//=============================================================================
/**
 * @file nimcp_lnn_numerical.h
 * @brief Numerical safety utilities for Liquid Neural Networks
 *
 * WHAT: Helper functions and macros for NaN/Inf detection and safe float operations
 * WHY:  LNNs involve exp(), tanh(), division - all prone to numerical issues
 * HOW:  Provide guards, clamps, and safe alternatives to standard operations
 */

#ifndef NIMCP_LNN_NUMERICAL_H
#define NIMCP_LNN_NUMERICAL_H

#include <math.h>
#include <stdbool.h>
#include <float.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Constants for Numerical Safety
 *===========================================================================*/

/** Maximum safe input to expf() to avoid overflow */
#define NIMCP_EXP_MAX_INPUT 87.0f

/** Minimum safe input to expf() to avoid underflow */
#define NIMCP_EXP_MIN_INPUT (-87.0f)

/** Small epsilon for division safety */
#define NIMCP_DIV_EPSILON 1e-7f

/** Large value threshold for detecting explosion */
#define NIMCP_EXPLOSION_THRESHOLD 1e6f

/** Small value threshold for detecting vanishing */
#define NIMCP_VANISHING_THRESHOLD 1e-10f

/*=============================================================================
 * Quick Check Macros
 *===========================================================================*/

/** Check if float value is NaN or Inf */
#define NIMCP_IS_INVALID_FLOAT(x) (isnan(x) || isinf(x))

/** Check if float value is valid (not NaN, not Inf) */
#define NIMCP_IS_VALID_FLOAT(x) (!isnan(x) && !isinf(x))

/** Guard clause - return fallback if value is invalid */
#define NIMCP_GUARD_FLOAT(x, fallback) \
    do { if (NIMCP_IS_INVALID_FLOAT(x)) return (fallback); } while(0)

/*=============================================================================
 * Safe Float Operations - Inline Functions
 *===========================================================================*/

/**
 * @brief Sanitize a float value, replacing NaN/Inf with default
 */
static inline float nimcp_safe_float(float x, float default_val) {
    return (isnan(x) || isinf(x)) ? default_val : x;
}

/**
 * @brief Sanitize float with zero as default
 */
static inline float nimcp_safe_float_zero(float x) {
    return (isnan(x) || isinf(x)) ? 0.0f : x;
}

/**
 * @brief Check if float is valid
 */
static inline bool nimcp_float_is_valid(float x) {
    return !isnan(x) && !isinf(x);
}

/*=============================================================================
 * Safe Mathematical Operations
 *===========================================================================*/

/**
 * @brief Safe exponential with input clamping
 */
static inline float nimcp_safe_exp(float x) {
    if (isnan(x)) return 1.0f;
    if (isinf(x)) return (x > 0) ? FLT_MAX : 0.0f;
    if (x > NIMCP_EXP_MAX_INPUT) x = NIMCP_EXP_MAX_INPUT;
    if (x < NIMCP_EXP_MIN_INPUT) x = NIMCP_EXP_MIN_INPUT;
    return expf(x);
}

/**
 * @brief Safe division with denominator protection
 */
static inline float nimcp_safe_div(float a, float b) {
    if (isnan(a) || isnan(b)) return 0.0f;
    if (isinf(a)) return (a > 0) ? FLT_MAX : -FLT_MAX;
    if (isinf(b)) return 0.0f;
    if (fabsf(b) < NIMCP_DIV_EPSILON) {
        b = (b >= 0.0f) ? NIMCP_DIV_EPSILON : -NIMCP_DIV_EPSILON;
    }
    return a / b;
}

/**
 * @brief Safe logarithm with input clamping
 */
static inline float nimcp_safe_log(float x) {
    if (isnan(x)) return 0.0f;
    if (isinf(x)) return (x > 0) ? FLT_MAX : -FLT_MAX;
    if (x <= 0.0f) x = NIMCP_VANISHING_THRESHOLD;
    return logf(x);
}

/**
 * @brief Safe square root with negative protection
 */
static inline float nimcp_safe_sqrt(float x) {
    if (isnan(x)) return 0.0f;
    if (isinf(x)) return (x > 0) ? sqrtf(FLT_MAX) : 0.0f;
    if (x < 0.0f) x = 0.0f;
    return sqrtf(x);
}

/*=============================================================================
 * Safe Activation Functions
 *===========================================================================*/

/**
 * @brief Safe sigmoid with overflow protection
 */
static inline float nimcp_safe_sigmoid(float x) {
    if (isnan(x)) return 0.5f;
    if (x > 20.0f) return 1.0f;
    if (x < -20.0f) return 0.0f;
    return 1.0f / (1.0f + nimcp_safe_exp(-x));
}

/**
 * @brief Safe tanh with overflow protection
 */
static inline float nimcp_safe_tanh(float x) {
    if (isnan(x)) return 0.0f;
    if (isinf(x)) return (x > 0) ? 1.0f : -1.0f;
    float result = tanhf(x);
    if (isnan(result)) return 0.0f;
    return result;
}

/**
 * @brief Safe ReLU
 */
static inline float nimcp_safe_relu(float x) {
    if (isnan(x) || isinf(x)) return 0.0f;
    return (x > 0.0f) ? x : 0.0f;
}

/**
 * @brief Safe softplus with overflow protection
 */
static inline float nimcp_safe_softplus(float x) {
    if (isnan(x)) return 0.0f;
    if (x > 20.0f) return x;
    if (x < -20.0f) return nimcp_safe_exp(x);
    return nimcp_safe_log(1.0f + nimcp_safe_exp(x));
}

/*=============================================================================
 * Array Validation Utilities
 *===========================================================================*/

/**
 * @brief Check array for any NaN or Inf values
 */
static inline bool nimcp_array_has_nan_inf(const float* data, size_t n) {
    if (!data || n == 0) return false;
    for (size_t i = 0; i < n; i++) {
        if (isnan(data[i]) || isinf(data[i])) return true;
    }
    return false;
}

/**
 * @brief Sanitize array by replacing NaN/Inf with default value
 */
static inline size_t nimcp_array_sanitize(float* data, size_t n, float default_val) {
    if (!data || n == 0) return 0;
    size_t replaced = 0;
    for (size_t i = 0; i < n; i++) {
        if (isnan(data[i]) || isinf(data[i])) {
            data[i] = default_val;
            replaced++;
        }
    }
    return replaced;
}

/**
 * @brief Clamp array values to safe range
 */
static inline size_t nimcp_array_clamp(float* data, size_t n, float min_val, float max_val) {
    if (!data || n == 0) return 0;
    size_t clamped = 0;
    for (size_t i = 0; i < n; i++) {
        if (isnan(data[i])) {
            data[i] = 0.0f;
            clamped++;
        } else if (isinf(data[i])) {
            data[i] = (data[i] > 0) ? max_val : min_val;
            clamped++;
        } else if (data[i] < min_val) {
            data[i] = min_val;
            clamped++;
        } else if (data[i] > max_val) {
            data[i] = max_val;
            clamped++;
        }
    }
    return clamped;
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_NUMERICAL_H */
