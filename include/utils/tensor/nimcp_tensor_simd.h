/**
 * @file nimcp_tensor_simd.h
 * @brief SIMD-Optimized Tensor Operations
 *
 * WHAT: Hardware-accelerated tensor operations using SIMD instructions
 * WHY:  Significantly faster element-wise operations, reductions, dot products
 * HOW:  Runtime dispatch to AVX2/AVX-512/NEON based on CPU capabilities
 *
 * ARCHITECTURE:
 *
 *   ┌──────────────────────────────────────────────────────────────┐
 *   │                    SIMD TENSOR OPERATIONS                      │
 *   │                                                                │
 *   │  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    │
 *   │  │  AVX-512     │    │    AVX2      │    │    NEON      │    │
 *   │  │  (512-bit)   │    │  (256-bit)   │    │  (128-bit)   │    │
 *   │  └──────────────┘    └──────────────┘    └──────────────┘    │
 *   │         │                  │                    │            │
 *   │         └──────────────────┼────────────────────┘            │
 *   │                            ▼                                  │
 *   │            ┌──────────────────────────────┐                  │
 *   │            │  Runtime Dispatch Function   │                  │
 *   │            │  (Based on SIMD detection)   │                  │
 *   │            └──────────────────────────────┘                  │
 *   │                            │                                  │
 *   │                            ▼                                  │
 *   │            ┌──────────────────────────────┐                  │
 *   │            │    Scalar Fallback           │                  │
 *   │            │  (Portable C implementation) │                  │
 *   │            └──────────────────────────────┘                  │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * PERFORMANCE CHARACTERISTICS:
 * - AVX-512: 16 floats/cycle for vectorized ops
 * - AVX2:    8 floats/cycle for vectorized ops
 * - NEON:    4 floats/cycle for vectorized ops
 * - Scalar:  1 float/cycle (fallback)
 *
 * THREAD SAFETY: All functions are thread-safe (no global state modified)
 *
 * POINTER ALIASING WARNING:
 * =========================
 * Functions marked with __restrict pointers MUST NOT receive overlapping
 * memory regions. The same array CANNOT be passed as both src and dst
 * for in-place operations that have separate src/dst parameters.
 *
 * UNDEFINED BEHAVIOR if aliased:
 *   float data[100];
 *   tensor_simd_add_f32(data, data, 100);  // WRONG: dst==src aliasing!
 *
 * CORRECT usage patterns:
 *   // Pattern 1: Separate arrays
 *   float dst[100], src[100];
 *   tensor_simd_add_f32(dst, src, 100);    // OK: no aliasing
 *
 *   // Pattern 2: Use true in-place functions (scalar variants)
 *   float data[100];
 *   tensor_simd_add_scalar_f32(data, 5.0f, 100);  // OK: single array
 *
 *   // Pattern 3: Copy first for safe "in-place" semantics
 *   float data[100], temp[100];
 *   memcpy(temp, data, sizeof(data));
 *   tensor_simd_add_f32(data, temp, 100);  // OK: no aliasing
 *
 * WHY this matters:
 * - SIMD instructions may read multiple elements ahead
 * - Writes to dst may corrupt not-yet-read src elements
 * - __restrict tells compiler no aliasing, enabling optimizations
 * - Violating __restrict is undefined behavior per C99/C11 standard
 *
 * @author NIMCP Development Team
 * @date 2025
 * @version 1.0
 */

#ifndef NIMCP_TENSOR_SIMD_H
#define NIMCP_TENSOR_SIMD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// SIMD Backend Selection
//=============================================================================

/**
 * @brief SIMD backend enumeration
 */
typedef enum {
    TENSOR_SIMD_NONE = 0,     /**< Scalar fallback */
    TENSOR_SIMD_SSE2,         /**< SSE2 (128-bit, x86) */
    TENSOR_SIMD_AVX,          /**< AVX (256-bit float, x86) */
    TENSOR_SIMD_AVX2,         /**< AVX2 (256-bit int/float, x86) */
    TENSOR_SIMD_AVX512,       /**< AVX-512 (512-bit, x86) */
    TENSOR_SIMD_NEON,         /**< NEON (128-bit, ARM) */
    TENSOR_SIMD_SVE           /**< SVE (scalable, ARM) */
} tensor_simd_backend_t;

/**
 * @brief Get current SIMD backend
 *
 * @return Currently selected SIMD backend based on CPU capabilities
 */
NIMCP_EXPORT tensor_simd_backend_t tensor_simd_get_backend(void);

/**
 * @brief Get SIMD backend name as string
 *
 * @param backend Backend type
 * @return Human-readable name
 */
NIMCP_EXPORT const char* tensor_simd_backend_name(tensor_simd_backend_t backend);

/**
 * @brief Initialize SIMD subsystem
 *
 * Called automatically on first use. Can be called explicitly for early init.
 *
 * @return 0 on success
 */
NIMCP_EXPORT int tensor_simd_init(void);

//=============================================================================
// Element-wise Operations (In-place)
//=============================================================================

/**
 * @brief SIMD-accelerated element-wise addition (in-place)
 *
 * Computes: dst[i] += src[i] for all i
 *
 * @param dst Destination array (modified in-place)
 * @param src Source array
 * @param n Number of elements
 *
 * PERFORMANCE:
 * - AVX-512: ~16 elements/cycle
 * - AVX2:    ~8 elements/cycle
 * - Scalar:  ~1 element/cycle
 */
NIMCP_EXPORT void tensor_simd_add_f32(float* __restrict dst,
                                       const float* __restrict src,
                                       size_t n);

/**
 * @brief SIMD-accelerated element-wise subtraction (in-place)
 *
 * Computes: dst[i] -= src[i] for all i
 *
 * @param dst Destination array (modified in-place)
 * @param src Source array
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_sub_f32(float* __restrict dst,
                                       const float* __restrict src,
                                       size_t n);

/**
 * @brief SIMD-accelerated element-wise multiplication (in-place)
 *
 * Computes: dst[i] *= src[i] for all i
 *
 * @param dst Destination array (modified in-place)
 * @param src Source array
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_mul_f32(float* __restrict dst,
                                       const float* __restrict src,
                                       size_t n);

/**
 * @brief SIMD-accelerated scalar addition (in-place)
 *
 * Computes: dst[i] += scalar for all i
 *
 * @param dst Destination array (modified in-place)
 * @param scalar Scalar value to add
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_add_scalar_f32(float* __restrict dst,
                                              float scalar,
                                              size_t n);

/**
 * @brief SIMD-accelerated scalar multiplication (in-place)
 *
 * Computes: dst[i] *= scalar for all i
 *
 * @param dst Destination array (modified in-place)
 * @param scalar Scalar multiplier
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_mul_scalar_f32(float* __restrict dst,
                                              float scalar,
                                              size_t n);

//=============================================================================
// Reduction Operations
//=============================================================================

/**
 * @brief SIMD-accelerated sum reduction
 *
 * Computes: sum(src[0..n-1])
 *
 * @param src Source array
 * @param n Number of elements
 * @return Sum of all elements
 *
 * NUMERICAL NOTE: Uses double accumulator for precision, even with float input
 */
NIMCP_EXPORT double tensor_simd_sum_f32(const float* __restrict src, size_t n);

/**
 * @brief SIMD-accelerated dot product
 *
 * Computes: sum(a[i] * b[i]) for all i
 *
 * @param a First array
 * @param b Second array
 * @param n Number of elements
 * @return Dot product
 *
 * NUMERICAL NOTE: Uses double accumulator for precision
 */
NIMCP_EXPORT double tensor_simd_dot_f32(const float* __restrict a,
                                         const float* __restrict b,
                                         size_t n);

/**
 * @brief SIMD-accelerated sum of squares (for Frobenius norm)
 *
 * Computes: sum(src[i]^2) for all i
 *
 * @param src Source array
 * @param n Number of elements
 * @return Sum of squares
 */
NIMCP_EXPORT double tensor_simd_sum_sq_f32(const float* __restrict src, size_t n);

/**
 * @brief SIMD-accelerated maximum reduction
 *
 * @param src Source array
 * @param n Number of elements
 * @return Maximum value
 */
NIMCP_EXPORT float tensor_simd_max_f32(const float* __restrict src, size_t n);

/**
 * @brief SIMD-accelerated minimum reduction
 *
 * @param src Source array
 * @param n Number of elements
 * @return Minimum value
 */
NIMCP_EXPORT float tensor_simd_min_f32(const float* __restrict src, size_t n);

//=============================================================================
// Activation Functions (Element-wise, In-place)
//=============================================================================

/**
 * @brief SIMD-accelerated ReLU (in-place)
 *
 * Computes: dst[i] = max(0, dst[i])
 *
 * @param dst Data array (modified in-place)
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_relu_f32(float* __restrict dst, size_t n);

/**
 * @brief SIMD-accelerated sigmoid approximation (in-place)
 *
 * Uses fast polynomial approximation for sigmoid
 *
 * @param dst Data array (modified in-place)
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_sigmoid_f32(float* __restrict dst, size_t n);

//=============================================================================
// Copy Operations
//=============================================================================

/**
 * @brief SIMD-accelerated memory copy for floats
 *
 * @param dst Destination array
 * @param src Source array
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_copy_f32(float* __restrict dst,
                                        const float* __restrict src,
                                        size_t n);

/**
 * @brief SIMD-accelerated memory fill
 *
 * @param dst Destination array
 * @param value Fill value
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_fill_f32(float* __restrict dst,
                                        float value,
                                        size_t n);

//=============================================================================
// Fused Operations (Multiple ops in single pass for better cache utilization)
//=============================================================================

/**
 * @brief SIMD-accelerated fused multiply-add (in-place)
 *
 * Computes: dst[i] = dst[i] * mul + add for all i
 *
 * Uses FMA instruction when available (AVX2+, NEON)
 *
 * @param dst Data array (modified in-place)
 * @param mul Multiplier
 * @param add Addend
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_fma_scalar_f32(float* __restrict dst,
                                              float mul,
                                              float add,
                                              size_t n);

/**
 * @brief SIMD-accelerated fused multiply-add with vectors (in-place)
 *
 * Computes: dst[i] = dst[i] * mul[i] + add[i] for all i
 *
 * @param dst Data array (modified in-place)
 * @param mul Multiplier array
 * @param add Addend array
 * @param n Number of elements
 */
NIMCP_EXPORT void tensor_simd_fma_f32(float* __restrict dst,
                                       const float* __restrict mul,
                                       const float* __restrict add,
                                       size_t n);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_TENSOR_SIMD_H
