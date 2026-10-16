/**
 * @file nimcp_neuralnet_simd.h
 * @brief SIMD-accelerated neural network operations
 * @version 2.7.0
 * @date 2025-11-04
 *
 * Enhancement 3: SIMD Vectorization
 *
 * WHAT: Vectorized neural network operations using AVX2/AVX-512
 * WHY: 4-8x performance improvement for forward pass
 * HOW: Runtime CPU detection, fallback to scalar
 *
 * Benefits:
 * - 4-8x faster inference
 * - Lower latency
 * - Energy efficient
 */

#ifndef NIMCP_NEURALNET_SIMD_H
#define NIMCP_NEURALNET_SIMD_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"

// Forward declaration
typedef struct neural_network_struct* neural_network_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// CPU Feature Detection
//=============================================================================

/**
 * @brief SIMD instruction set support
 */
typedef enum {
    NIMCP_SIMD_NONE = 0,      ///< No SIMD support (scalar fallback)
    NIMCP_SIMD_SSE2 = 1,      ///< SSE2 (4-wide float, baseline x86-64)
    NIMCP_SIMD_AVX = 2,       ///< AVX (8-wide float)
    NIMCP_SIMD_AVX2 = 3,      ///< AVX2 (8-wide float with FMA)
    NIMCP_SIMD_AVX512 = 4     ///< AVX-512 (16-wide float)
} nimcp_simd_level_t;

/**
 * @brief CPU capabilities
 */
typedef struct {
    bool has_sse2;            ///< SSE2 support
    bool has_avx;             ///< AVX support
    bool has_avx2;            ///< AVX2 support
    bool has_fma;             ///< FMA (fused multiply-add) support
    bool has_avx512f;         ///< AVX-512 Foundation support
    bool has_avx512bw;        ///< AVX-512 Byte/Word support

    nimcp_simd_level_t best_level;  ///< Best SIMD level available
    const char* cpu_brand;    ///< CPU brand string (e.g., "Intel Core i9")
} nimcp_cpu_caps_t;

/**
 * @brief Detect CPU capabilities
 *
 * Performs runtime CPU feature detection using CPUID.
 * Results are cached for performance.
 *
 * @param caps Output CPU capabilities
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_cpu_detect(nimcp_cpu_caps_t* caps);

/**
 * @brief Get cached CPU capabilities
 *
 * Returns cached result from first nimcp_cpu_detect() call.
 *
 * @return CPU capabilities
 */
NIMCP_EXPORT const nimcp_cpu_caps_t* nimcp_cpu_get_caps(void);

//=============================================================================
// SIMD Vector Operations
//=============================================================================

/**
 * @brief Vectorized dot product
 *
 * Computes: result = sum(a[i] * b[i])
 * Auto-selects best SIMD implementation.
 *
 * @param a First vector
 * @param b Second vector
 * @param n Vector length (must be multiple of 8 for best performance)
 * @return Dot product
 */
NIMCP_EXPORT float nimcp_simd_dot_product(
    const float* a,
    const float* b,
    uint32_t n
);

/**
 * @brief Vectorized vector addition
 *
 * Computes: result[i] = a[i] + b[i]
 *
 * @param a First vector
 * @param b Second vector
 * @param result Output vector
 * @param n Vector length
 */
NIMCP_EXPORT void nimcp_simd_vector_add(
    const float* a,
    const float* b,
    float* result,
    uint32_t n
);

/**
 * @brief Vectorized vector multiplication
 *
 * Computes: result[i] = a[i] * b[i]
 *
 * @param a First vector
 * @param b Second vector
 * @param result Output vector
 * @param n Vector length
 */
NIMCP_EXPORT void nimcp_simd_vector_mul(
    const float* a,
    const float* b,
    float* result,
    uint32_t n
);

/**
 * @brief Vectorized ReLU activation
 *
 * Computes: output[i] = max(0, input[i])
 *
 * @param input Input vector
 * @param output Output vector
 * @param n Vector length
 */
NIMCP_EXPORT void nimcp_simd_relu(
    const float* input,
    float* output,
    uint32_t n
);

/**
 * @brief Vectorized sigmoid activation
 *
 * Computes: output[i] = 1 / (1 + exp(-input[i]))
 *
 * @param input Input vector
 * @param output Output vector
 * @param n Vector length
 */
NIMCP_EXPORT void nimcp_simd_sigmoid(
    const float* input,
    float* output,
    uint32_t n
);

//=============================================================================
// SIMD Matrix Operations
//=============================================================================

/**
 * @brief Vectorized matrix-vector multiplication
 *
 * Computes: output = matrix * input
 * where matrix is [m × n] and input is [n]
 *
 * Auto-selects best SIMD implementation based on CPU.
 *
 * @param matrix Weight matrix (row-major order)
 * @param input Input vector
 * @param output Output vector
 * @param m Number of output neurons
 * @param n Number of input neurons
 */
NIMCP_EXPORT void nimcp_simd_matvec_mul(
    const float* matrix,
    const float* input,
    float* output,
    uint32_t m,
    uint32_t n
);

/**
 * @brief Vectorized matrix-matrix multiplication
 *
 * Computes: C = A * B
 * where A is [m × k], B is [k × n], C is [m × n]
 *
 * @param a Matrix A (row-major)
 * @param b Matrix B (row-major)
 * @param c Output matrix C (row-major)
 * @param m Rows in A
 * @param k Columns in A / Rows in B
 * @param n Columns in B
 */
NIMCP_EXPORT void nimcp_simd_matmat_mul(
    const float* a,
    const float* b,
    float* c,
    uint32_t m,
    uint32_t k,
    uint32_t n
);

//=============================================================================
// SIMD-Optimized Neural Network Forward Pass
//=============================================================================

/**
 * @brief SIMD-accelerated forward pass
 *
 * Replaces neural_network_forward() with vectorized implementation.
 * Auto-detects CPU capabilities and selects best path.
 *
 * @param network Neural network
 * @param input Input vector
 * @param input_size Input dimension
 * @param output Output vector (caller allocates)
 * @param output_size Output dimension
 * @return Number of active neurons
 */
NIMCP_EXPORT uint32_t neural_network_forward_simd(
    neural_network_t network,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
);

//=============================================================================
// Specific SIMD Implementations (Internal)
//=============================================================================

#ifdef __AVX2__
/**
 * @brief AVX2 matrix-vector multiplication (8-wide float)
 *
 * @param matrix Weight matrix
 * @param input Input vector
 * @param output Output vector
 * @param m Number of outputs
 * @param n Number of inputs
 */
void nimcp_simd_matvec_mul_avx2(
    const float* matrix,
    const float* input,
    float* output,
    uint32_t m,
    uint32_t n
);
#endif

#ifdef __AVX512F__
/**
 * @brief AVX-512 matrix-vector multiplication (16-wide float)
 *
 * @param matrix Weight matrix
 * @param input Input vector
 * @param output Output vector
 * @param m Number of outputs
 * @param n Number of inputs
 */
void nimcp_simd_matvec_mul_avx512(
    const float* matrix,
    const float* input,
    float* output,
    uint32_t m,
    uint32_t n
);
#endif

/**
 * @brief Scalar fallback matrix-vector multiplication
 *
 * Used when no SIMD support available.
 *
 * @param matrix Weight matrix
 * @param input Input vector
 * @param output Output vector
 * @param m Number of outputs
 * @param n Number of inputs
 */
void nimcp_simd_matvec_mul_scalar(
    const float* matrix,
    const float* input,
    float* output,
    uint32_t m,
    uint32_t n
);

//=============================================================================
// Benchmarking & Profiling
//=============================================================================

/**
 * @brief Benchmark SIMD performance
 *
 * Measures throughput of different SIMD implementations.
 *
 * @param matrix_size Square matrix size (e.g., 1000 for 1000×1000)
 * @param iterations Number of iterations
 * @return GFLOPS (billion floating-point operations per second)
 */
NIMCP_EXPORT float nimcp_simd_benchmark(
    uint32_t matrix_size,
    uint32_t iterations
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_NEURALNET_SIMD_H
