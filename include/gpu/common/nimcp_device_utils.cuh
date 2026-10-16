/**
 * @file nimcp_device_utils.cuh
 * @brief Common Device Utility Functions
 *
 * WHAT: Shared device-side utility functions
 * WHY:  Eliminate duplication of helper functions across modules
 * HOW:  Inline __device__ functions for common operations
 *
 * USAGE:
 *   #include "gpu/common/nimcp_device_utils.cuh"
 *
 *   __global__ void my_kernel(float* x) {
 *       x[idx] = nimcp_device_clamp(x[idx], 0.0f, 1.0f);
 *   }
 *
 * CONSOLIDATES DUPLICATES FROM:
 *   - perception/nimcp_dragonfly_vision_kernels.cu
 *   - memory/nimcp_memory_consolidation_kernels.cu
 *   - glial/nimcp_myelin_kernels.cu
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_DEVICE_UTILS_CUH
#define NIMCP_DEVICE_UTILS_CUH

#ifdef __CUDACC__

#include <cuda_runtime.h>
#include <math.h>

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_PI        3.14159265358979323846f
#define NIMCP_2PI       6.28318530717958647692f
#define NIMCP_PI_2      1.57079632679489661923f
#define NIMCP_E         2.71828182845904523536f
#define NIMCP_SQRT2     1.41421356237309504880f
#define NIMCP_SQRT2_INV 0.70710678118654752440f
#define NIMCP_LN2       0.69314718055994530942f
#define NIMCP_LN10      2.30258509299404568402f
#define NIMCP_EPS       1e-7f

//=============================================================================
// Clamping and Saturation
//=============================================================================

/**
 * @brief Clamp value to range [min_val, max_val]
 *
 * @param x Input value
 * @param min_val Minimum bound
 * @param max_val Maximum bound
 * @return Clamped value
 */
__device__ __forceinline__ float nimcp_device_clamp(float x, float min_val, float max_val)
{
    return fminf(fmaxf(x, min_val), max_val);
}

/**
 * @brief Clamp value to [0, 1]
 */
__device__ __forceinline__ float nimcp_device_saturate(float x)
{
    return nimcp_device_clamp(x, 0.0f, 1.0f);
}

/**
 * @brief Clamp integer value
 */
__device__ __forceinline__ int nimcp_device_clamp_int(int x, int min_val, int max_val)
{
    return min(max(x, min_val), max_val);
}

//=============================================================================
// Interpolation
//=============================================================================

/**
 * @brief Linear interpolation: a + t * (b - a)
 *
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor [0, 1]
 * @return Interpolated value
 */
__device__ __forceinline__ float nimcp_device_lerp(float a, float b, float t)
{
    return a + t * (b - a);
}

/**
 * @brief Smooth step (Hermite interpolation)
 *
 * Returns smooth transition from 0 to 1 as x goes from edge0 to edge1.
 *
 * @param edge0 Lower edge
 * @param edge1 Upper edge
 * @param x Input value
 * @return Smooth interpolation in [0, 1]
 */
__device__ __forceinline__ float nimcp_device_smoothstep(float edge0, float edge1, float x)
{
    float t = nimcp_device_saturate((x - edge0) / (edge1 - edge0));
    return t * t * (3.0f - 2.0f * t);
}

/**
 * @brief Smoother step (Ken Perlin's improved version)
 *
 * C2 continuous (second derivative also continuous).
 */
__device__ __forceinline__ float nimcp_device_smootherstep(float edge0, float edge1, float x)
{
    float t = nimcp_device_saturate((x - edge0) / (edge1 - edge0));
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

/**
 * @brief Inverse lerp: find t such that lerp(a, b, t) = x
 *
 * @param a Start value
 * @param b End value
 * @param x Value to find parameter for
 * @return Interpolation parameter t
 */
__device__ __forceinline__ float nimcp_device_inverse_lerp(float a, float b, float x)
{
    return (b - a != 0.0f) ? (x - a) / (b - a) : 0.0f;
}

/**
 * @brief Remap value from one range to another
 *
 * @param x Input value
 * @param in_min Input range minimum
 * @param in_max Input range maximum
 * @param out_min Output range minimum
 * @param out_max Output range maximum
 * @return Remapped value
 */
__device__ __forceinline__ float nimcp_device_remap(
    float x, float in_min, float in_max, float out_min, float out_max)
{
    float t = nimcp_device_inverse_lerp(in_min, in_max, x);
    return nimcp_device_lerp(out_min, out_max, t);
}

//=============================================================================
// Exponential and Logarithmic
//=============================================================================

/**
 * @brief Safe log (avoids log(0))
 *
 * @param x Input value
 * @param eps Minimum value to prevent log(0)
 * @return log(max(x, eps))
 */
__device__ __forceinline__ float nimcp_device_safe_log(float x, float eps = NIMCP_EPS)
{
    return logf(fmaxf(x, eps));
}

/**
 * @brief Safe divide (avoids division by zero)
 *
 * @param a Numerator
 * @param b Denominator
 * @param eps Minimum denominator value
 * @return a / max(|b|, eps) * sign(b)
 */
__device__ __forceinline__ float nimcp_device_safe_div(float a, float b, float eps = NIMCP_EPS)
{
    return a / (fabsf(b) > eps ? b : copysignf(eps, b));
}

/**
 * @brief Exponential decay: x * exp(-dt / tau)
 */
__device__ __forceinline__ float nimcp_device_exp_decay(float x, float dt, float tau)
{
    return x * expf(-dt / tau);
}

/**
 * @brief Log-sum-exp for numerical stability
 *
 * Computes log(exp(a) + exp(b)) in a numerically stable way.
 */
__device__ __forceinline__ float nimcp_device_log_sum_exp(float a, float b)
{
    float m = fmaxf(a, b);
    return m + log1pf(expf(-fabsf(a - b)));
}

//=============================================================================
// Sign and Comparison
//=============================================================================

/**
 * @brief Sign function: -1, 0, or +1
 */
__device__ __forceinline__ float nimcp_device_sign(float x)
{
    return (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f);
}

/**
 * @brief Step function: 0 if x < edge, 1 otherwise
 */
__device__ __forceinline__ float nimcp_device_step(float edge, float x)
{
    return (x >= edge) ? 1.0f : 0.0f;
}

/**
 * @brief Absolute difference
 */
__device__ __forceinline__ float nimcp_device_abs_diff(float a, float b)
{
    return fabsf(a - b);
}

/**
 * @brief Check if values are approximately equal
 */
__device__ __forceinline__ bool nimcp_device_approx_equal(float a, float b, float eps = NIMCP_EPS)
{
    return fabsf(a - b) <= eps * fmaxf(1.0f, fmaxf(fabsf(a), fabsf(b)));
}

//=============================================================================
// Normalization
//=============================================================================

/**
 * @brief Normalize value to [0, 1] given min/max
 */
__device__ __forceinline__ float nimcp_device_normalize(float x, float min_val, float max_val)
{
    return (max_val - min_val != 0.0f) ? (x - min_val) / (max_val - min_val) : 0.0f;
}

/**
 * @brief Fast inverse square root (Quake III algorithm, for fun/compatibility)
 *
 * Use rsqrtf() for production code.
 */
__device__ __forceinline__ float nimcp_device_fast_rsqrt(float x)
{
    return rsqrtf(x);  // Use CUDA's optimized version
}

/**
 * @brief Safe normalize (handles zero length)
 */
__device__ __forceinline__ float nimcp_device_safe_normalize_scalar(float x, float length)
{
    return (length > NIMCP_EPS) ? x / length : 0.0f;
}

//=============================================================================
// Distance and Similarity
//=============================================================================

/**
 * @brief Squared Euclidean distance (for single components)
 */
__device__ __forceinline__ float nimcp_device_sq_dist(float a, float b)
{
    float d = a - b;
    return d * d;
}

/**
 * @brief Gaussian kernel: exp(-x^2 / (2 * sigma^2))
 */
__device__ __forceinline__ float nimcp_device_gaussian(float x, float sigma)
{
    float normalized = x / sigma;
    return expf(-0.5f * normalized * normalized);
}

/**
 * @brief Cauchy/Lorentzian kernel: 1 / (1 + (x/gamma)^2)
 */
__device__ __forceinline__ float nimcp_device_cauchy(float x, float gamma)
{
    float normalized = x / gamma;
    return 1.0f / (1.0f + normalized * normalized);
}

//=============================================================================
// Bit Manipulation
//=============================================================================

/**
 * @brief Extract bits from integer
 */
__device__ __forceinline__ unsigned int nimcp_device_extract_bits(
    unsigned int value, int start, int len)
{
    return (value >> start) & ((1u << len) - 1);
}

/**
 * @brief Insert bits into integer
 */
__device__ __forceinline__ unsigned int nimcp_device_insert_bits(
    unsigned int value, unsigned int bits, int start, int len)
{
    unsigned int mask = ((1u << len) - 1) << start;
    return (value & ~mask) | ((bits << start) & mask);
}

/**
 * @brief Count leading zeros
 */
__device__ __forceinline__ int nimcp_device_clz(unsigned int x)
{
    return __clz(x);
}

/**
 * @brief Population count (number of set bits)
 */
__device__ __forceinline__ int nimcp_device_popcount(unsigned int x)
{
    return __popc(x);
}

//=============================================================================
// Random and Noise Helpers
//=============================================================================

/**
 * @brief Simple hash for pseudo-random generation
 *
 * Wang hash for integer -> integer mapping.
 */
__device__ __forceinline__ unsigned int nimcp_device_hash(unsigned int x)
{
    x = (x ^ 61) ^ (x >> 16);
    x = x + (x << 3);
    x = x ^ (x >> 4);
    x = x * 0x27d4eb2d;
    x = x ^ (x >> 15);
    return x;
}

/**
 * @brief Convert hash to float in [0, 1)
 */
__device__ __forceinline__ float nimcp_device_hash_to_float(unsigned int hash)
{
    return (float)hash / 4294967296.0f;
}

//=============================================================================
// GPU RNG Device Functions
//=============================================================================
// NOTE: For production code, prefer using the central GPU statistics RNG:
//   - stats_gpu_rng_create/destroy/reseed (see gpu/statistics/nimcp_statistics_gpu.h)
//   - nimcp_stats_gpu_sample_normal/uniform for batch sampling
//
// These device functions provide lightweight inline RNG for cases where:
//   1. Per-thread state is managed manually (e.g., kernel_init_rng pattern)
//   2. Simple stochastic operations in kernels
//   3. cuRAND dependency should be avoided
//
// For cuRAND-based kernels, use the standard pattern:
//   curandState state;
//   curand_init(seed, idx, 0, &state);
//   float uniform = curand_uniform(&state);
//   float normal = curand_normal(&state);
//=============================================================================

/**
 * @brief Linear Congruential Generator (LCG) for device-side RNG
 *
 * Generates uniform random float in [0, 1).
 * Parameters from Numerical Recipes (period 2^32).
 *
 * Usage:
 *   uint32_t state = seed + threadIdx.x * 1237u;
 *   float r = nimcp_device_lcg_uniform(&state);
 *
 * @param state Pointer to RNG state (modified in-place)
 * @return Uniform random value in [0, 1)
 *
 * @note For high-quality randomness, prefer cuRAND. This LCG is suitable
 *       for simple stochastic noise but not for Monte Carlo simulations.
 */
__device__ __forceinline__ float nimcp_device_lcg_uniform(uint32_t* state)
{
    *state = *state * 1664525u + 1013904223u;
    return (float)(*state) / 4294967296.0f;
}

/**
 * @brief Box-Muller transform for Gaussian noise using LCG
 *
 * Generates standard normal random value (mean=0, stddev=1).
 *
 * @param state Pointer to RNG state (modified in-place)
 * @return Standard normal random value
 *
 * @note Consumes two uniform samples per call.
 */
__device__ __forceinline__ float nimcp_device_lcg_normal(uint32_t* state)
{
    float u1 = nimcp_device_lcg_uniform(state);
    float u2 = nimcp_device_lcg_uniform(state);
    // Avoid log(0)
    u1 = fmaxf(u1, 1e-10f);
    return sqrtf(-2.0f * logf(u1)) * cosf(NIMCP_2PI * u2);
}

/**
 * @brief Gaussian noise with specified standard deviation using LCG
 *
 * @param state Pointer to RNG state (modified in-place)
 * @param stddev Standard deviation
 * @return Normal random value with mean=0 and given stddev
 */
__device__ __forceinline__ float nimcp_device_lcg_gaussian(uint32_t* state, float stddev)
{
    return nimcp_device_lcg_normal(state) * stddev;
}

/**
 * @brief Exponential random value using LCG
 *
 * @param state Pointer to RNG state (modified in-place)
 * @param lambda Rate parameter (1/mean)
 * @return Exponential random value
 */
__device__ __forceinline__ float nimcp_device_lcg_exponential(uint32_t* state, float lambda)
{
    float u = nimcp_device_lcg_uniform(state);
    // Avoid log(0)
    u = fmaxf(u, 1e-10f);
    return -logf(u) / lambda;
}

/**
 * @brief Initialize per-thread RNG state for deterministic replay
 *
 * Combines seed with thread index for unique but reproducible sequences.
 *
 * @param seed Base seed
 * @param thread_idx Thread index (use blockIdx.x * blockDim.x + threadIdx.x)
 * @return Initial RNG state for this thread
 */
__device__ __forceinline__ uint32_t nimcp_device_rng_init(uint32_t seed, uint32_t thread_idx)
{
    // Combine seed and thread index with different multipliers to avoid correlation
    return seed + thread_idx * 1237u + nimcp_device_hash(seed ^ thread_idx);
}

#endif // __CUDACC__

#endif // NIMCP_DEVICE_UTILS_CUH
