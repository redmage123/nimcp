//=============================================================================
// nimcp_math_helpers.h - Common Math Helper Functions
//=============================================================================
/**
 * @file nimcp_math_helpers.h
 * @brief Shared inline math utility functions used across the codebase.
 *
 * WHAT: Clamping, safe logarithm, sigmoid, and entropy helpers.
 * WHY:  Eliminates dozens of duplicate static definitions scattered across
 *       source files (clamp, clampf, safe_log, compute_entropy, sigmoid, etc.)
 * HOW:  Header-only via static inline to avoid ODR violations.
 *
 * USAGE:  #include "utils/math/nimcp_math_helpers.h"
 *         float x = nimcp_clampf(val, 0.0f, 1.0f);
 *
 * @author NIMCP Development Team
 * @date 2026-02-26
 */

#ifndef NIMCP_MATH_HELPERS_H
#define NIMCP_MATH_HELPERS_H

#include <math.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Clamp a float value to [min_val, max_val] range.
 */
static inline float nimcp_clampf(float x, float min_val, float max_val) {
    if (x < min_val) return min_val;
    if (x > max_val) return max_val;
    return x;
}

/**
 * @brief Clamp a float to [0, 1].
 */
static inline float nimcp_clamp01(float x) {
    return nimcp_clampf(x, 0.0f, 1.0f);
}

/**
 * @brief Safe logarithm -- returns log(max(x, epsilon)).
 *
 * Prevents -inf / NaN from log(0) or log(negative).
 */
static inline float nimcp_safe_logf(float x) {
    return logf(x > 1e-10f ? x : 1e-10f);
}

/**
 * @brief Compute Shannon entropy from probability distribution.
 * @param probs Array of probabilities (must sum to ~1.0)
 * @param n     Number of elements
 * @return Entropy in nats (natural log base)
 */
static inline float nimcp_entropy(const float* probs, uint32_t n) {
    float h = 0.0f;
    for (uint32_t i = 0; i < n; i++) {
        if (probs[i] > 1e-10f) {
            h -= probs[i] * logf(probs[i]);
        }
    }
    return h;
}

/**
 * @brief Standard sigmoid function: 1 / (1 + exp(-x)).
 */
static inline float nimcp_sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Guard an EMA value against NaN/Inf corruption.
 *
 * WHAT: Resets an EMA accumulator to a fallback value if it becomes non-finite.
 * WHY:  A single NaN/Inf in an EMA permanently corrupts the running average
 *       because NaN * decay + anything = NaN.
 * HOW:  Call after every EMA update: NIMCP_EMA_GUARD(ema_var, fallback_value);
 *
 * @param ema_var   The EMA variable (lvalue) to check.
 * @param fallback  The value to reset to if ema_var is NaN or Inf.
 */
#define NIMCP_EMA_GUARD(ema_var, fallback) \
    do { if (!isfinite(ema_var)) (ema_var) = (fallback); } while (0)

/**
 * @brief Guard an EMA value, resetting to zero on NaN/Inf.
 */
#define NIMCP_EMA_GUARD_ZERO(ema_var) \
    NIMCP_EMA_GUARD(ema_var, 0.0f)

/**
 * @brief Safe exponential that guards against overflow producing Inf.
 *
 * WHAT: Clamps input to [-88, 88] before calling expf().
 * WHY:  expf(89) = Inf in single precision; expf(-89) = 0 (underflow is safe).
 * HOW:  Returns expf(clamped_x) which is always finite and positive.
 */
static inline float nimcp_safe_expf(float x) {
    if (x > 88.0f) x = 88.0f;
    if (x < -88.0f) x = -88.0f;
    return expf(x);
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_MATH_HELPERS_H */
