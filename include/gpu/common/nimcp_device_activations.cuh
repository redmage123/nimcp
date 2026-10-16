/**
 * @file nimcp_device_activations.cuh
 * @brief Shared Device Activation Functions for GPU Kernels
 *
 * WHAT: Common device-side activation function implementations
 * WHY:  Eliminate duplication across 10+ GPU kernel files
 * HOW:  Inline __device__ functions usable from any CUDA kernel
 *
 * USAGE:
 *   #include "gpu/common/nimcp_device_activations.cuh"
 *
 *   __global__ void my_kernel(float* x, int n) {
 *       int idx = blockIdx.x * blockDim.x + threadIdx.x;
 *       if (idx < n) x[idx] = nimcp_device_sigmoid(x[idx]);
 *   }
 *
 * CONSOLIDATES DUPLICATES FROM:
 *   - lnn/nimcp_lnn_kernels.cu
 *   - lnn/nimcp_lnn_ode_kernels.cu
 *   - neuron/nimcp_gpu_kernels.cu
 *   - snn/nimcp_snn_kernels.cu
 *   - memory/nimcp_memory_consolidation_kernels.cu
 *   - perception/nimcp_dragonfly_vision_kernels.cu
 *   - cognitive/nimcp_jepa_gpu.cu
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_DEVICE_ACTIVATIONS_CUH
#define NIMCP_DEVICE_ACTIVATIONS_CUH

#ifdef __CUDACC__

#include <cuda_runtime.h>
#include <math.h>

//=============================================================================
// Constants
//=============================================================================

#define NIMCP_GELU_COEFF 0.044715f
#define NIMCP_SQRT_2_PI  2.5066282746310002f  // sqrt(2*pi)
#define NIMCP_SQRT_2_INV 0.7071067811865476f  // 1/sqrt(2)

//=============================================================================
// Standard Activation Functions
//=============================================================================

/**
 * @brief Sigmoid activation: 1 / (1 + exp(-x))
 *
 * @param x Input value
 * @return Sigmoid output in [0, 1]
 */
__device__ __forceinline__ float nimcp_device_sigmoid(float x)
{
    return 1.0f / (1.0f + expf(-x));
}

/**
 * @brief Fast sigmoid approximation using tanh
 *
 * Slightly faster but less accurate than standard sigmoid.
 * sigmoid(x) ≈ 0.5 * (1 + tanh(x * 0.5))
 *
 * @param x Input value
 * @return Approximate sigmoid output
 */
__device__ __forceinline__ float nimcp_device_sigmoid_fast(float x)
{
    return 0.5f * (1.0f + tanhf(x * 0.5f));
}

/**
 * @brief Hyperbolic tangent activation
 *
 * @param x Input value
 * @return Tanh output in [-1, 1]
 */
__device__ __forceinline__ float nimcp_device_tanh(float x)
{
    return tanhf(x);
}

/**
 * @brief Fast tanh approximation
 *
 * Uses rational approximation for speed.
 * Accurate to ~1e-4 for |x| < 3.
 *
 * @param x Input value
 * @return Approximate tanh output
 */
__device__ __forceinline__ float nimcp_device_tanh_fast(float x)
{
    float x2 = x * x;
    float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
    return a / b;
}

/**
 * @brief ReLU activation: max(0, x)
 *
 * @param x Input value
 * @return ReLU output (non-negative)
 */
__device__ __forceinline__ float nimcp_device_relu(float x)
{
    return fmaxf(0.0f, x);
}

/**
 * @brief ReLU6 activation: min(max(0, x), 6)
 *
 * @param x Input value
 * @return ReLU6 output in [0, 6]
 */
__device__ __forceinline__ float nimcp_device_relu6(float x)
{
    return fminf(fmaxf(0.0f, x), 6.0f);
}

/**
 * @brief Leaky ReLU activation
 *
 * @param x Input value
 * @param alpha Negative slope (default 0.01)
 * @return Leaky ReLU output
 */
__device__ __forceinline__ float nimcp_device_leaky_relu(float x, float alpha = 0.01f)
{
    return (x > 0.0f) ? x : alpha * x;
}

/**
 * @brief ELU (Exponential Linear Unit) activation
 *
 * @param x Input value
 * @param alpha ELU alpha parameter (default 1.0)
 * @return ELU output
 */
__device__ __forceinline__ float nimcp_device_elu(float x, float alpha = 1.0f)
{
    return (x > 0.0f) ? x : alpha * (expf(x) - 1.0f);
}

/**
 * @brief GELU (Gaussian Error Linear Unit) activation
 *
 * Exact: x * 0.5 * (1 + erf(x / sqrt(2)))
 * Using tanh approximation for speed.
 *
 * @param x Input value
 * @return GELU output
 */
__device__ __forceinline__ float nimcp_device_gelu(float x)
{
    // Tanh approximation: 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
    float x3 = x * x * x;
    float inner = NIMCP_SQRT_2_INV * NIMCP_SQRT_2_PI * (x + NIMCP_GELU_COEFF * x3);
    return 0.5f * x * (1.0f + tanhf(inner));
}

/**
 * @brief GELU exact using error function
 *
 * More accurate but slower than tanh approximation.
 *
 * @param x Input value
 * @return Exact GELU output
 */
__device__ __forceinline__ float nimcp_device_gelu_exact(float x)
{
    return 0.5f * x * (1.0f + erff(x * NIMCP_SQRT_2_INV));
}

/**
 * @brief SiLU/Swish activation: x * sigmoid(x)
 *
 * Also known as Swish activation.
 *
 * @param x Input value
 * @return SiLU output
 */
__device__ __forceinline__ float nimcp_device_silu(float x)
{
    return x * nimcp_device_sigmoid(x);
}

/**
 * @brief Softplus activation: log(1 + exp(x))
 *
 * Smooth approximation of ReLU.
 *
 * @param x Input value
 * @return Softplus output (always positive)
 */
__device__ __forceinline__ float nimcp_device_softplus(float x)
{
    // Numerically stable: use log1p for small x
    if (x > 20.0f) return x;  // Avoid overflow
    if (x < -20.0f) return 0.0f;
    return log1pf(expf(x));
}

/**
 * @brief Mish activation: x * tanh(softplus(x))
 *
 * @param x Input value
 * @return Mish output
 */
__device__ __forceinline__ float nimcp_device_mish(float x)
{
    return x * tanhf(nimcp_device_softplus(x));
}

/**
 * @brief Hard sigmoid: clip((x + 3) / 6, 0, 1)
 *
 * Fast piecewise linear approximation of sigmoid.
 *
 * @param x Input value
 * @return Hard sigmoid output in [0, 1]
 */
__device__ __forceinline__ float nimcp_device_hard_sigmoid(float x)
{
    return fminf(fmaxf((x + 3.0f) / 6.0f, 0.0f), 1.0f);
}

/**
 * @brief Hard swish: x * hard_sigmoid(x)
 *
 * Fast approximation of swish/silu.
 *
 * @param x Input value
 * @return Hard swish output
 */
__device__ __forceinline__ float nimcp_device_hard_swish(float x)
{
    return x * nimcp_device_hard_sigmoid(x);
}

//=============================================================================
// Specialized Neural Activations
//=============================================================================

/**
 * @brief Surrogate sigmoid for SNN gradient approximation
 *
 * Used in spiking neural networks for differentiable spike function.
 * Derivative is non-zero for backward pass.
 *
 * @param x Input value
 * @param beta Sharpness parameter (default 5.0)
 * @return Surrogate sigmoid output
 */
__device__ __forceinline__ float nimcp_device_surrogate_sigmoid(float x, float beta = 5.0f)
{
    return 1.0f / (1.0f + expf(-beta * x));
}

/**
 * @brief Surrogate sigmoid derivative
 *
 * @param x Input value
 * @param beta Sharpness parameter
 * @return Derivative value for backprop
 */
__device__ __forceinline__ float nimcp_device_surrogate_sigmoid_grad(float x, float beta = 5.0f)
{
    float s = nimcp_device_surrogate_sigmoid(x, beta);
    return beta * s * (1.0f - s);
}

/**
 * @brief Fast exponential decay
 *
 * Used for membrane potential decay, synaptic traces, etc.
 *
 * @param x Current value
 * @param dt Time step
 * @param tau Time constant
 * @return Decayed value
 */
__device__ __forceinline__ float nimcp_device_decay(float x, float dt, float tau)
{
    return x * expf(-dt / tau);
}

//=============================================================================
// Activation Derivatives (for backpropagation)
//=============================================================================

/**
 * @brief Sigmoid derivative given sigmoid output
 *
 * @param s Sigmoid output (not input!)
 * @return Derivative: s * (1 - s)
 */
__device__ __forceinline__ float nimcp_device_sigmoid_grad(float s)
{
    return s * (1.0f - s);
}

/**
 * @brief Tanh derivative given tanh output
 *
 * @param t Tanh output (not input!)
 * @return Derivative: 1 - t^2
 */
__device__ __forceinline__ float nimcp_device_tanh_grad(float t)
{
    return 1.0f - t * t;
}

/**
 * @brief ReLU derivative
 *
 * @param x Input value
 * @return Derivative: 1 if x > 0, else 0
 */
__device__ __forceinline__ float nimcp_device_relu_grad(float x)
{
    return (x > 0.0f) ? 1.0f : 0.0f;
}

/**
 * @brief Leaky ReLU derivative
 *
 * @param x Input value
 * @param alpha Negative slope
 * @return Derivative: 1 if x > 0, else alpha
 */
__device__ __forceinline__ float nimcp_device_leaky_relu_grad(float x, float alpha = 0.01f)
{
    return (x > 0.0f) ? 1.0f : alpha;
}

//=============================================================================
// FP16 Variants (if CUDA supports half precision)
//=============================================================================

#if defined(__CUDA_ARCH__) && __CUDA_ARCH__ >= 530
#include <cuda_fp16.h>

__device__ __forceinline__ __half nimcp_device_sigmoid_fp16(__half x)
{
    return __hdiv(__float2half(1.0f), __hadd(__float2half(1.0f), hexp(__hneg(x))));
}

__device__ __forceinline__ __half nimcp_device_relu_fp16(__half x)
{
    return __hmax(__float2half(0.0f), x);
}

__device__ __forceinline__ __half nimcp_device_tanh_fp16(__half x)
{
    // Use float conversion for tanh (no native half tanh)
    return __float2half(tanhf(__half2float(x)));
}

#endif // FP16 support

#endif // __CUDACC__

#endif // NIMCP_DEVICE_ACTIVATIONS_CUH
