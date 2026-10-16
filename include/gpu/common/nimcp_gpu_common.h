/**
 * @file nimcp_gpu_common.h
 * @brief Master Include for GPU Common Infrastructure
 *
 * WHAT: Single header to include all GPU common utilities
 * WHY:  Simplify includes for GPU kernel development
 * HOW:  Includes all common headers in correct order
 *
 * USAGE:
 *   // In your .cu file:
 *   #include "gpu/common/nimcp_gpu_common.h"
 *
 *   // Now you have access to:
 *   // - nimcp_device_sigmoid(), nimcp_device_relu(), etc.
 *   // - nimcp_warp_reduce_sum(), nimcp_block_reduce_max(), etc.
 *   // - nimcp_device_clamp(), nimcp_device_lerp(), etc.
 *   // - NIMCP_CUDA_CHECK(), NIMCP_CUDA_GRID_SIZE(), etc.
 *
 * CONTENTS:
 *   - nimcp_cuda_utils.h        : Error checking macros, launch helpers
 *   - nimcp_device_activations.cuh : Activation functions (sigmoid, relu, etc.)
 *   - nimcp_device_utils.cuh   : Utility functions (clamp, lerp, etc.)
 *   - nimcp_warp_primitives.cuh : Warp/block reductions
 *
 * MIGRATION GUIDE:
 *   Old code:
 *     #define CUDA_CHECK(x) { cudaError_t e = x; if (e) return false; }
 *     __device__ float sigmoid(float x) { return 1.0f / (1.0f + expf(-x)); }
 *     __device__ float warp_reduce_sum(float val) { ... }
 *
 *   New code:
 *     #include "gpu/common/nimcp_gpu_common.h"
 *     // Use: NIMCP_CUDA_CHECK(x)
 *     // Use: nimcp_device_sigmoid(x)
 *     // Use: nimcp_warp_reduce_sum(val)
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_GPU_COMMON_H
#define NIMCP_GPU_COMMON_H

// Standard CUDA error handling and utilities (works in .c and .cu files)
#include "gpu/common/nimcp_cuda_utils.h"

#ifdef __CUDACC__
// CUDA-only headers (require nvcc compilation)

// Device-side activation functions
#include "gpu/common/nimcp_device_activations.cuh"

// Device-side utility functions
#include "gpu/common/nimcp_device_utils.cuh"

// Warp and block reduction primitives
#include "gpu/common/nimcp_warp_primitives.cuh"

#endif // __CUDACC__

//=============================================================================
// Version Information
//=============================================================================

#define NIMCP_GPU_COMMON_VERSION_MAJOR 1
#define NIMCP_GPU_COMMON_VERSION_MINOR 0
#define NIMCP_GPU_COMMON_VERSION_PATCH 0

//=============================================================================
// Quick Reference
//=============================================================================

/*
 * ACTIVATION FUNCTIONS (nimcp_device_activations.cuh):
 *   nimcp_device_sigmoid(x)
 *   nimcp_device_sigmoid_fast(x)
 *   nimcp_device_tanh(x)
 *   nimcp_device_tanh_fast(x)
 *   nimcp_device_relu(x)
 *   nimcp_device_relu6(x)
 *   nimcp_device_leaky_relu(x, alpha)
 *   nimcp_device_elu(x, alpha)
 *   nimcp_device_gelu(x)
 *   nimcp_device_gelu_exact(x)
 *   nimcp_device_silu(x)
 *   nimcp_device_softplus(x)
 *   nimcp_device_mish(x)
 *   nimcp_device_hard_sigmoid(x)
 *   nimcp_device_hard_swish(x)
 *   nimcp_device_surrogate_sigmoid(x, beta)
 *   nimcp_device_decay(x, dt, tau)
 *
 * DERIVATIVES:
 *   nimcp_device_sigmoid_grad(s)
 *   nimcp_device_tanh_grad(t)
 *   nimcp_device_relu_grad(x)
 *   nimcp_device_leaky_relu_grad(x, alpha)
 *   nimcp_device_surrogate_sigmoid_grad(x, beta)
 *
 * UTILITY FUNCTIONS (nimcp_device_utils.cuh):
 *   nimcp_device_clamp(x, min, max)
 *   nimcp_device_saturate(x)
 *   nimcp_device_lerp(a, b, t)
 *   nimcp_device_smoothstep(edge0, edge1, x)
 *   nimcp_device_smootherstep(edge0, edge1, x)
 *   nimcp_device_inverse_lerp(a, b, x)
 *   nimcp_device_remap(x, in_min, in_max, out_min, out_max)
 *   nimcp_device_safe_log(x, eps)
 *   nimcp_device_safe_div(a, b, eps)
 *   nimcp_device_exp_decay(x, dt, tau)
 *   nimcp_device_log_sum_exp(a, b)
 *   nimcp_device_sign(x)
 *   nimcp_device_step(edge, x)
 *   nimcp_device_normalize(x, min, max)
 *   nimcp_device_gaussian(x, sigma)
 *   nimcp_device_cauchy(x, gamma)
 *   nimcp_device_hash(x)
 *
 * WARP PRIMITIVES (nimcp_warp_primitives.cuh):
 *   nimcp_warp_reduce_sum(val)
 *   nimcp_warp_reduce_max(val)
 *   nimcp_warp_reduce_min(val)
 *   nimcp_warp_reduce_prod(val)
 *   nimcp_warp_reduce_argmax(val, idx, out_idx)
 *   nimcp_warp_reduce_sum_int(val)
 *   nimcp_warp_reduce_max_int(val)
 *   nimcp_warp_reduce_min_int(val)
 *   nimcp_block_reduce_sum(val, shared)
 *   nimcp_block_reduce_max(val, shared)
 *   nimcp_block_reduce_min(val, shared)
 *   nimcp_warp_broadcast(val)
 *   nimcp_warp_broadcast_lane(val, lane)
 *   nimcp_warp_scan_inclusive(val)
 *   nimcp_warp_scan_exclusive(val)
 *   nimcp_warp_all(predicate)
 *   nimcp_warp_any(predicate)
 *   nimcp_warp_count(predicate)
 *   nimcp_atomic_max_float(addr, val)
 *   nimcp_atomic_min_float(addr, val)
 *
 * ERROR CHECKING (nimcp_cuda_utils.h):
 *   NIMCP_CUDA_CHECK(call)           // Return false on error
 *   NIMCP_CUDA_CHECK_NULL(call)      // Return NULL on error
 *   NIMCP_CUDA_CHECK_ERROR(call)     // Return error code on error
 *   NIMCP_CUDA_CHECK_GOTO(call, lbl) // Goto label on error
 *   NIMCP_CUDA_CHECK_WARN(call)      // Warn but continue
 *   NIMCP_CUDA_CHECK_LAST()          // Check kernel launch errors
 *   NIMCP_CUDA_SYNC_CHECK()          // Sync and check errors
 *   NIMCP_CUBLAS_CHECK(call)         // cuBLAS error checking
 *   NIMCP_CUSPARSE_CHECK(call)       // cuSPARSE error checking
 *   NIMCP_CURAND_CHECK(call)         // cuRAND error checking
 *
 * LAUNCH CONFIGURATION:
 *   NIMCP_CUDA_BLOCK_SIZE            // Default block size (256)
 *   NIMCP_CUDA_WARP_SIZE             // Warp size (32)
 *   NIMCP_CUDA_GRID_SIZE(n, block)   // Calculate grid size
 *   nimcp_cuda_optimal_block_size()  // Occupancy-based block size
 *
 * CONSTANTS:
 *   NIMCP_PI, NIMCP_2PI, NIMCP_PI_2
 *   NIMCP_E, NIMCP_SQRT2, NIMCP_LN2
 *   NIMCP_WARP_SIZE, NIMCP_FULL_WARP_MASK
 */

#endif // NIMCP_GPU_COMMON_H
