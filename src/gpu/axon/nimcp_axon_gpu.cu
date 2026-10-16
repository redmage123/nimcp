/**
 * @file nimcp_axon_gpu.cu
 * @brief GPU CUDA Kernels for Axon Signal Propagation
 *
 * WHAT: CUDA kernels for GPU-accelerated axon operations
 * WHY:  GPU acceleration for parallel signal propagation and velocity calculations
 * HOW:  Custom kernels using nimcp_gpu_tensor_t for all data storage
 *
 * BIOLOGICAL BASIS:
 * =================
 * Axons transmit action potentials through:
 * - Saltatory conduction in myelinated axons (jumping between nodes)
 * - Continuous conduction in unmyelinated axons
 * - Velocity proportional to diameter (Hursh's law for myelinated)
 * - Refractory periods preventing immediate re-firing
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Large-scale neural networks benefit from:
 * - Parallel signal propagation across thousands of axons
 * - Batch velocity calculations for all axons simultaneously
 * - Efficient myelination effect computation via tensor multiply
 * - Reduced CPU-GPU synchronization through fused kernels
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gpu/axon/nimcp_axon_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "utils/memory/nimcp_memory.h"

#define LOG_MODULE "AXON_GPU"

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

// Physical constants
#define MIN_VELOCITY 0.1f            // Minimum velocity (m/s)
#define MAX_VELOCITY 120.0f          // Maximum biological velocity (m/s)
#define VELOCITY_COEFF_UNMYELINATED 1.0f   // m/s per sqrt(um)
#define VELOCITY_COEFF_MYELINATED 6.0f     // m/s per um diameter
#define ATP_SPIKE_COST 0.01f         // ATP consumed per spike
#define ATP_REGEN_RATE 0.001f        // ATP regeneration per ms

//=============================================================================
// CUDA Kernels: Signal Propagation
//=============================================================================

/**
 * @brief Parallel signal propagation kernel
 *
 * WHAT: Advance signal positions along all axons
 * WHY:  Core simulation step - signals move through segments
 * HOW:  Each thread processes one axon, updates signal positions
 *
 * Memory access pattern optimized for coalesced reads/writes
 */
__global__ void kernel_axon_propagate(
    float* __restrict__ signals,           // [N_axons, N_segments]
    float* __restrict__ positions,         // [N_axons, N_segments]
    const float* __restrict__ seg_velocities, // [N_axons, N_segments]
    const float* __restrict__ seg_lengths, // [N_axons, N_segments]
    const uint8_t* __restrict__ active,    // [N_axons]
    uint32_t num_axons,
    uint32_t num_segments,
    float dt
) {
    uint32_t axon_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (axon_idx >= num_axons) return;

    // Skip inactive axons
    if (!active[axon_idx]) return;

    // Calculate base indices
    uint32_t base_idx = axon_idx * num_segments;

    // Propagate signal through segments
    for (uint32_t seg = 0; seg < num_segments; seg++) {
        uint32_t idx = base_idx + seg;

        // Only propagate non-zero signals
        if (signals[idx] > 0.0f) {
            // Calculate distance traveled this timestep
            // distance = velocity * dt (convert ms to seconds for m/s velocity)
            float distance = seg_velocities[idx] * dt * 0.001f;  // m/s * ms = um

            // Update position within segment
            positions[idx] += distance;

            // Check if signal has moved to next segment
            if (positions[idx] >= seg_lengths[idx] && seg < num_segments - 1) {
                // Transfer signal to next segment
                float overflow = positions[idx] - seg_lengths[idx];
                positions[idx] = 0.0f;
                signals[idx] = 0.0f;  // Clear current segment

                // Activate next segment
                uint32_t next_idx = base_idx + seg + 1;
                signals[next_idx] = signals[idx];  // Transfer amplitude
                positions[next_idx] = overflow;
            }
        }
    }
}

/**
 * @brief Initiate spikes on specified axons
 *
 * Each thread handles one spike initiation
 */
__global__ void kernel_initiate_spikes(
    float* __restrict__ signals,
    float* __restrict__ positions,
    float* __restrict__ refractory,
    uint8_t* __restrict__ active,
    uint64_t* __restrict__ spike_times,
    float* __restrict__ atp_levels,
    const uint32_t* __restrict__ axon_indices,
    const float* __restrict__ amplitudes,
    uint32_t count,
    uint32_t num_segments,
    uint64_t current_time,
    float refractory_period_ms
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= count) return;

    uint32_t axon_idx = axon_indices[idx];

    // Check if axon is available (not refractory)
    if (refractory[axon_idx] > 0.0f) return;

    // Check ATP level (metabolic gating)
    if (atp_levels[axon_idx] < 0.1f) return;

    // Get amplitude (default 1.0 if not provided)
    float amplitude = amplitudes ? amplitudes[idx] : 1.0f;

    // Initialize signal at first segment
    uint32_t base_idx = axon_idx * num_segments;
    signals[base_idx] = amplitude;
    positions[base_idx] = 0.0f;

    // Set axon as active
    active[axon_idx] = 1;

    // Set refractory period
    refractory[axon_idx] = refractory_period_ms;

    // Record spike time
    spike_times[axon_idx] = current_time;

    // Consume ATP
    atp_levels[axon_idx] -= ATP_SPIKE_COST;
    if (atp_levels[axon_idx] < 0.0f) atp_levels[axon_idx] = 0.0f;
}

/**
 * @brief Check for spike arrivals at terminals
 *
 * Uses atomic operations to build arrival list
 */
__global__ void kernel_check_arrivals(
    const float* __restrict__ signals,
    const float* __restrict__ positions,
    const float* __restrict__ seg_lengths,
    uint8_t* __restrict__ active,
    uint32_t* __restrict__ arrived_indices,
    uint32_t* __restrict__ arrival_counter,
    uint32_t num_axons,
    uint32_t num_segments,
    uint32_t max_arrivals
) {
    uint32_t axon_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (axon_idx >= num_axons) return;

    // Only check active axons
    if (!active[axon_idx]) return;

    // Check last segment
    uint32_t last_seg_idx = axon_idx * num_segments + (num_segments - 1);

    // Check if signal has reached end of last segment
    if (signals[last_seg_idx] > 0.0f &&
        positions[last_seg_idx] >= seg_lengths[last_seg_idx]) {

        // Atomically add to arrival list
        uint32_t slot = atomicAdd(arrival_counter, 1);
        if (slot < max_arrivals) {
            arrived_indices[slot] = axon_idx;
        }

        // Clear signal and deactivate
        // Note: Using volatile to ensure visibility
        active[axon_idx] = 0;
    }
}

//=============================================================================
// CUDA Kernels: Velocity Calculation
//=============================================================================

/**
 * @brief Update conduction velocities based on myelination and diameter
 *
 * WHAT: Calculate effective velocity for each axon
 * WHY:  Velocity depends on morphology and myelination
 * HOW:  Hursh's law for myelinated, sqrt(diameter) for unmyelinated
 *
 * BIOLOGICAL:
 * - Unmyelinated: v = k1 * sqrt(diameter) [approximately 1 m/s for 1um]
 * - Myelinated: v = k2 * diameter * myelination_factor [up to 120 m/s]
 */
__global__ void kernel_axon_velocity_update(
    float* __restrict__ velocities,
    const float* __restrict__ diameters,
    const float* __restrict__ myelination,
    uint32_t num_axons,
    uint32_t num_segments,
    float base_velocity,
    float myelin_multiplier
) {
    uint32_t axon_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (axon_idx >= num_axons) return;

    float diameter = diameters[axon_idx];

    // Calculate mean myelination across segments
    float mean_myelin = 0.0f;
    uint32_t base_idx = axon_idx * num_segments;
    for (uint32_t seg = 0; seg < num_segments; seg++) {
        mean_myelin += myelination[base_idx + seg];
    }
    mean_myelin /= (float)num_segments;

    float velocity;
    if (mean_myelin > 0.1f) {
        // Myelinated: v proportional to diameter, scaled by myelination
        velocity = VELOCITY_COEFF_MYELINATED * diameter *
                   (0.1f + 0.9f * mean_myelin) * myelin_multiplier / 50.0f;
    } else {
        // Unmyelinated: v proportional to sqrt(diameter)
        velocity = VELOCITY_COEFF_UNMYELINATED * sqrtf(diameter);
    }

    // Clamp to valid range
    velocity = fmaxf(velocity, MIN_VELOCITY);
    velocity = fminf(velocity, MAX_VELOCITY);

    velocities[axon_idx] = velocity;
}

/**
 * @brief Update segment-level velocities
 *
 * Each segment can have different myelination level
 */
__global__ void kernel_segment_velocity_update(
    float* __restrict__ seg_velocities,
    const float* __restrict__ diameters,
    const float* __restrict__ myelination,
    const float* __restrict__ seg_lengths,
    float* __restrict__ seg_delays,
    uint32_t num_axons,
    uint32_t num_segments,
    float myelin_multiplier
) {
    uint32_t axon_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (axon_idx >= num_axons) return;

    float diameter = diameters[axon_idx];
    uint32_t base_idx = axon_idx * num_segments;

    float cumulative_delay = 0.0f;

    for (uint32_t seg = 0; seg < num_segments; seg++) {
        uint32_t idx = base_idx + seg;
        float myelin = myelination[idx];

        float velocity;
        if (myelin > 0.1f) {
            // Myelinated segment (internode)
            velocity = VELOCITY_COEFF_MYELINATED * diameter *
                       myelin * myelin_multiplier / 50.0f;
        } else {
            // Node of Ranvier or unmyelinated
            velocity = VELOCITY_COEFF_UNMYELINATED * sqrtf(diameter);
        }

        velocity = fmaxf(velocity, MIN_VELOCITY);
        velocity = fminf(velocity, MAX_VELOCITY);

        seg_velocities[idx] = velocity;

        // Calculate cumulative delay
        // delay (ms) = length (um) / velocity (m/s) / 1000
        float segment_delay = seg_lengths[idx] / (velocity * 1000.0f);
        cumulative_delay += segment_delay;
        seg_delays[idx] = cumulative_delay;
    }
}

/**
 * @brief Apply myelination effects - tensor multiply for speedup
 *
 * WHAT: Multiply velocities by myelination factor
 * WHY:  Myelination increases velocity by up to 50x
 * HOW:  Element-wise tensor operation
 */
__global__ void kernel_axon_myelination_effect(
    float* __restrict__ velocities,
    const float* __restrict__ myelination,
    uint32_t num_axons,
    uint32_t num_segments,
    float myelin_multiplier,
    float max_velocity
) {
    uint32_t axon_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (axon_idx >= num_axons) return;

    // Calculate mean myelination
    float mean_myelin = 0.0f;
    uint32_t base_idx = axon_idx * num_segments;
    for (uint32_t seg = 0; seg < num_segments; seg++) {
        mean_myelin += myelination[base_idx + seg];
    }
    mean_myelin /= (float)num_segments;

    // Apply myelination multiplier
    float velocity = velocities[axon_idx];
    float myelin_factor = 1.0f + (myelin_multiplier - 1.0f) * mean_myelin;
    velocity *= myelin_factor;

    // Clamp to maximum
    velocity = fminf(velocity, max_velocity);

    velocities[axon_idx] = velocity;
}

//=============================================================================
// CUDA Kernels: Refractory and Activity
//=============================================================================

/**
 * @brief Update refractory states
 */
__global__ void kernel_update_refractory(
    float* __restrict__ refractory,
    uint32_t num_axons,
    float dt
) {
    uint32_t axon_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (axon_idx >= num_axons) return;

    if (refractory[axon_idx] > 0.0f) {
        refractory[axon_idx] -= dt;
        if (refractory[axon_idx] < 0.0f) {
            refractory[axon_idx] = 0.0f;
        }
    }
}

/**
 * @brief Update ATP levels
 */
__global__ void kernel_update_atp(
    float* __restrict__ atp_levels,
    uint32_t num_axons,
    float dt,
    float regeneration_rate
) {
    uint32_t axon_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (axon_idx >= num_axons) return;

    atp_levels[axon_idx] += regeneration_rate * dt;
    if (atp_levels[axon_idx] > 1.0f) {
        atp_levels[axon_idx] = 1.0f;
    }
}

/**
 * @brief Get availability mask (non-refractory axons)
 */
__global__ void kernel_get_available(
    const float* __restrict__ refractory,
    uint8_t* __restrict__ available,
    uint32_t num_axons
) {
    uint32_t axon_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (axon_idx >= num_axons) return;

    available[axon_idx] = (refractory[axon_idx] <= 0.0f) ? 1 : 0;
}

/**
 * @brief Fused simulation step kernel
 *
 * Combines propagation, refractory update, and ATP regeneration
 */
__global__ void kernel_axon_step(
    float* __restrict__ signals,
    float* __restrict__ positions,
    const float* __restrict__ seg_velocities,
    const float* __restrict__ seg_lengths,
    uint8_t* __restrict__ active,
    float* __restrict__ refractory,
    float* __restrict__ atp_levels,
    uint32_t num_axons,
    uint32_t num_segments,
    float dt,
    float atp_regen_rate
) {
    uint32_t axon_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (axon_idx >= num_axons) return;

    // Update refractory
    if (refractory[axon_idx] > 0.0f) {
        refractory[axon_idx] -= dt;
        if (refractory[axon_idx] < 0.0f) {
            refractory[axon_idx] = 0.0f;
        }
    }

    // Update ATP
    atp_levels[axon_idx] += atp_regen_rate * dt;
    if (atp_levels[axon_idx] > 1.0f) {
        atp_levels[axon_idx] = 1.0f;
    }

    // Propagate signal if active
    if (active[axon_idx]) {
        uint32_t base_idx = axon_idx * num_segments;

        for (uint32_t seg = 0; seg < num_segments; seg++) {
            uint32_t idx = base_idx + seg;

            if (signals[idx] > 0.0f) {
                float distance = seg_velocities[idx] * dt * 0.001f;
                positions[idx] += distance;

                if (positions[idx] >= seg_lengths[idx]) {
                    if (seg < num_segments - 1) {
                        // Transfer to next segment
                        float overflow = positions[idx] - seg_lengths[idx];
                        float amp = signals[idx];
                        signals[idx] = 0.0f;
                        positions[idx] = 0.0f;
                        signals[idx + 1] = amp;
                        positions[idx + 1] = overflow;
                    }
                }
            }
        }
    }
}

//=============================================================================
// API Implementation: Configuration
//=============================================================================

extern "C" axon_gpu_config_t axon_gpu_default_config(void) {
    axon_gpu_config_t config;
    config.max_axons = 100000;
    config.max_segments = 64;
    config.max_batch_size = 10000;
    config.enable_async_transfer = true;
    config.enable_biophysics = false;
    config.refractory_period_ms = 1.0f;
    config.base_velocity_ms = 1.0f;
    config.myelin_multiplier = 50.0f;
    return config;
}

//=============================================================================
// API Implementation: Lifecycle
//=============================================================================

extern "C" axon_gpu_context_t* axon_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const axon_gpu_config_t* config
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!gpu_ctx) {
        LOG_ERROR("NULL GPU context");
        return NULL;
    }

    axon_gpu_context_t* ctx = (axon_gpu_context_t*)nimcp_calloc(1, sizeof(axon_gpu_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate Axon GPU context");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->config = config ? *config : axon_gpu_default_config();

    // Create CUDA stream
    cudaStream_t stream;
    cudaError_t err = cudaStreamCreate(&stream);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to create CUDA stream: %s", cudaGetErrorString(err));
        nimcp_free(ctx);
        return NULL;
    }
    ctx->stream = stream;

    // Initialize all tensor pointers to NULL
    ctx->positions = NULL;
    ctx->velocities = NULL;
    ctx->myelination = NULL;
    ctx->signals = NULL;
    ctx->delays = NULL;
    ctx->refractory = NULL;
    ctx->diameters = NULL;
    ctx->lengths = NULL;
    ctx->active = NULL;
    ctx->spike_times = NULL;
    ctx->atp_levels = NULL;
    ctx->seg_lengths = NULL;
    ctx->seg_velocities = NULL;
    ctx->seg_delays = NULL;
    ctx->source_neurons = NULL;
    ctx->target_synapses = NULL;

    ctx->num_axons = 0;
    ctx->num_segments = 0;

    LOG_INFO("GPU Axon context created (max_axons: %u, max_segments: %u)",
             ctx->config.max_axons, ctx->config.max_segments);

    return ctx;
}

extern "C" void axon_gpu_destroy(axon_gpu_context_t* ctx) {
    if (!ctx) return;

    // Synchronize and destroy stream
    if (ctx->stream) {
        cudaStreamSynchronize((cudaStream_t)ctx->stream);
        cudaStreamDestroy((cudaStream_t)ctx->stream);
    }

    // Destroy all tensors
    if (ctx->positions) nimcp_gpu_tensor_destroy(ctx->positions);
    if (ctx->velocities) nimcp_gpu_tensor_destroy(ctx->velocities);
    if (ctx->myelination) nimcp_gpu_tensor_destroy(ctx->myelination);
    if (ctx->signals) nimcp_gpu_tensor_destroy(ctx->signals);
    if (ctx->delays) nimcp_gpu_tensor_destroy(ctx->delays);
    if (ctx->refractory) nimcp_gpu_tensor_destroy(ctx->refractory);
    if (ctx->diameters) nimcp_gpu_tensor_destroy(ctx->diameters);
    if (ctx->lengths) nimcp_gpu_tensor_destroy(ctx->lengths);
    if (ctx->active) nimcp_gpu_tensor_destroy(ctx->active);
    if (ctx->spike_times) nimcp_gpu_tensor_destroy(ctx->spike_times);
    if (ctx->atp_levels) nimcp_gpu_tensor_destroy(ctx->atp_levels);
    if (ctx->seg_lengths) nimcp_gpu_tensor_destroy(ctx->seg_lengths);
    if (ctx->seg_velocities) nimcp_gpu_tensor_destroy(ctx->seg_velocities);
    if (ctx->seg_delays) nimcp_gpu_tensor_destroy(ctx->seg_delays);
    if (ctx->source_neurons) nimcp_gpu_tensor_destroy(ctx->source_neurons);
    if (ctx->target_synapses) nimcp_gpu_tensor_destroy(ctx->target_synapses);

    nimcp_free(ctx);
    LOG_DEBUG("GPU Axon context destroyed");
}

extern "C" bool axon_gpu_synchronize(axon_gpu_context_t* ctx) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize((cudaStream_t)ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Tensor Initialization
//=============================================================================

extern "C" bool axon_gpu_init_tensors(
    axon_gpu_context_t* ctx,
    uint32_t num_axons,
    uint32_t num_segments
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || num_axons == 0 || num_segments == 0) return false;
    if (num_axons > ctx->config.max_axons) {
        LOG_ERROR("Too many axons: %u > %u", num_axons, ctx->config.max_axons);
        return false;
    }
    if (num_segments > ctx->config.max_segments) {
        LOG_ERROR("Too many segments: %u > %u", num_segments, ctx->config.max_segments);
        return false;
    }

    ctx->num_axons = num_axons;
    ctx->num_segments = num_segments;

    // Dimensions for 1D tensors [num_axons]
    size_t dims_1d[1] = { num_axons };

    // Dimensions for 2D tensors [num_axons, num_segments]
    size_t dims_2d[2] = { num_axons, num_segments };

    // Create 2D tensors (float32)
    ctx->positions = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->myelination = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->signals = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->seg_lengths = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->seg_velocities = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->seg_delays = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_2d, 2, NIMCP_GPU_PRECISION_FP32);

    // Create 1D tensors (float32)
    ctx->velocities = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->delays = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->refractory = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->diameters = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->lengths = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);
    ctx->atp_levels = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_FP32);

    // Create 1D tensors (uint8 for active flags)
    ctx->active = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_INT8);

    // Create 1D tensors (uint64 for spike times)
    // Note: Using UINT32 as placeholder since there's no UINT64 precision
    ctx->spike_times = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_UINT32);

    // Create connectivity tensors (uint32)
    ctx->source_neurons = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_UINT32);
    ctx->target_synapses = nimcp_gpu_tensor_create(ctx->gpu_ctx, dims_1d, 1, NIMCP_GPU_PRECISION_UINT32);

    // Verify all allocations
    if (!ctx->positions || !ctx->myelination || !ctx->signals ||
        !ctx->velocities || !ctx->delays || !ctx->refractory ||
        !ctx->diameters || !ctx->lengths || !ctx->active ||
        !ctx->spike_times || !ctx->atp_levels ||
        !ctx->seg_lengths || !ctx->seg_velocities || !ctx->seg_delays ||
        !ctx->source_neurons || !ctx->target_synapses) {
        LOG_ERROR("Failed to allocate GPU tensors");
        axon_gpu_destroy(ctx);
        return false;
    }

    // Initialize tensors to zero/defaults
    nimcp_gpu_zeros(ctx->gpu_ctx, ctx->positions);
    nimcp_gpu_zeros(ctx->gpu_ctx, ctx->signals);
    nimcp_gpu_zeros(ctx->gpu_ctx, ctx->refractory);
    nimcp_gpu_zeros(ctx->gpu_ctx, ctx->active);

    // Initialize ATP to 1.0 (full)
    nimcp_gpu_ones(ctx->gpu_ctx, ctx->atp_levels);

    LOG_INFO("GPU Axon tensors initialized: %u axons x %u segments", num_axons, num_segments);
    return true;
}

extern "C" bool axon_gpu_upload_properties(
    axon_gpu_context_t* ctx,
    const float* diameters,
    const float* lengths,
    const float* myelination,
    uint32_t num_axons
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !diameters || !lengths || !myelination) return false;
    if (num_axons > ctx->num_axons) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    // Upload diameters
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->diameters->data, diameters,
                               num_axons * sizeof(float),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Upload lengths
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->lengths->data, lengths,
                               num_axons * sizeof(float),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Upload myelination [num_axons * num_segments]
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->myelination->data, myelination,
                               num_axons * ctx->num_segments * sizeof(float),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Calculate segment lengths (uniform distribution for now)
    float* seg_lengths_host = (float*)nimcp_malloc(num_axons * ctx->num_segments * sizeof(float));
    for (uint32_t a = 0; a < num_axons; a++) {
        float seg_len = lengths[a] / (float)ctx->num_segments;
        for (uint32_t s = 0; s < ctx->num_segments; s++) {
            seg_lengths_host[a * ctx->num_segments + s] = seg_len;
        }
    }
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->seg_lengths->data, seg_lengths_host,
                               num_axons * ctx->num_segments * sizeof(float),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    nimcp_free(seg_lengths_host);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    // Update velocities based on new properties
    return axon_gpu_update_velocities(ctx);
}

extern "C" bool axon_gpu_upload_connectivity(
    axon_gpu_context_t* ctx,
    const uint32_t* source_neurons,
    const uint32_t* target_synapses,
    uint32_t num_axons
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !source_neurons || !target_synapses) return false;
    if (num_axons > ctx->num_axons) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->source_neurons->data, source_neurons,
                               num_axons * sizeof(uint32_t),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->target_synapses->data, target_synapses,
                               num_axons * sizeof(uint32_t),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Signal Propagation
//=============================================================================

extern "C" bool axon_gpu_propagate(
    axon_gpu_context_t* ctx,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->num_axons == 0) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_axon_propagate<<<GRID_SIZE(ctx->num_axons), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->signals->data,
        (float*)ctx->positions->data,
        (float*)ctx->seg_velocities->data,
        (float*)ctx->seg_lengths->data,
        (uint8_t*)ctx->active->data,
        ctx->num_axons,
        ctx->num_segments,
        dt
    );

    ctx->total_updates++;
    return true;
}

extern "C" bool axon_gpu_initiate_spikes(
    axon_gpu_context_t* ctx,
    const uint32_t* axon_indices,
    const float* amplitudes,
    uint32_t count,
    uint64_t current_time
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !axon_indices || count == 0) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    // Upload indices
    uint32_t* d_indices;
    NIMCP_CUDA_RECOVER(cudaMallocAsync(&d_indices, count * sizeof(uint32_t), stream), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_indices, axon_indices, count * sizeof(uint32_t),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    // Upload amplitudes if provided
    float* d_amplitudes = NULL;
    if (amplitudes) {
        NIMCP_CUDA_RECOVER(cudaMallocAsync(&d_amplitudes, count * sizeof(float), stream), GPU_ERROR_OUT_OF_MEMORY);
        NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_amplitudes, amplitudes, count * sizeof(float),
                                   cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    }

    kernel_initiate_spikes<<<GRID_SIZE(count), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->signals->data,
        (float*)ctx->positions->data,
        (float*)ctx->refractory->data,
        (uint8_t*)ctx->active->data,
        (uint64_t*)ctx->spike_times->data,
        (float*)ctx->atp_levels->data,
        d_indices,
        d_amplitudes,
        count,
        ctx->num_segments,
        current_time,
        ctx->config.refractory_period_ms
    );

    // Cleanup
    NIMCP_CUDA_RECOVER(cudaFreeAsync(d_indices, stream), GPU_ERROR_OUT_OF_MEMORY);
    if (d_amplitudes) {
        NIMCP_CUDA_RECOVER(cudaFreeAsync(d_amplitudes, stream), GPU_ERROR_OUT_OF_MEMORY);
    }

    ctx->total_spikes += count;
    return true;
}

extern "C" bool axon_gpu_check_arrivals(
    axon_gpu_context_t* ctx,
    uint64_t current_time,
    uint32_t* arrived_indices,
    uint32_t max_arrivals,
    uint32_t* arrival_count
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !arrived_indices || !arrival_count) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    // Allocate device memory for results
    uint32_t* d_arrived;
    uint32_t* d_counter;
    NIMCP_CUDA_RECOVER(cudaMallocAsync(&d_arrived, max_arrivals * sizeof(uint32_t), stream), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMallocAsync(&d_counter, sizeof(uint32_t), stream), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemsetAsync(d_counter, 0, sizeof(uint32_t), stream), GPU_ERROR_CUDA_RUNTIME);

    kernel_check_arrivals<<<GRID_SIZE(ctx->num_axons), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->signals->data,
        (float*)ctx->positions->data,
        (float*)ctx->seg_lengths->data,
        (uint8_t*)ctx->active->data,
        d_arrived,
        d_counter,
        ctx->num_axons,
        ctx->num_segments,
        max_arrivals
    );

    // Download results
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(arrival_count, d_counter, sizeof(uint32_t),
                               cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);

    if (*arrival_count > 0) {
        uint32_t copy_count = (*arrival_count > max_arrivals) ? max_arrivals : *arrival_count;
        NIMCP_CUDA_RECOVER(cudaMemcpy(arrived_indices, d_arrived, copy_count * sizeof(uint32_t),
                              cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    }

    NIMCP_CUDA_RECOVER(cudaFreeAsync(d_arrived, stream), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaFreeAsync(d_counter, stream), GPU_ERROR_OUT_OF_MEMORY);

    return true;
}

//=============================================================================
// API Implementation: Velocity and Myelination
//=============================================================================

extern "C" bool axon_gpu_update_velocities(
    axon_gpu_context_t* ctx
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->num_axons == 0) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    // Update per-axon velocities
    kernel_axon_velocity_update<<<GRID_SIZE(ctx->num_axons), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->velocities->data,
        (float*)ctx->diameters->data,
        (float*)ctx->myelination->data,
        ctx->num_axons,
        ctx->num_segments,
        ctx->config.base_velocity_ms,
        ctx->config.myelin_multiplier
    );

    // Update per-segment velocities
    kernel_segment_velocity_update<<<GRID_SIZE(ctx->num_axons), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->seg_velocities->data,
        (float*)ctx->diameters->data,
        (float*)ctx->myelination->data,
        (float*)ctx->seg_lengths->data,
        (float*)ctx->seg_delays->data,
        ctx->num_axons,
        ctx->num_segments,
        ctx->config.myelin_multiplier
    );

    return true;
}

extern "C" bool axon_gpu_apply_myelination(
    axon_gpu_context_t* ctx
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->num_axons == 0) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_axon_myelination_effect<<<GRID_SIZE(ctx->num_axons), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->velocities->data,
        (float*)ctx->myelination->data,
        ctx->num_axons,
        ctx->num_segments,
        ctx->config.myelin_multiplier,
        MAX_VELOCITY
    );

    return true;
}

extern "C" bool axon_gpu_update_myelination(
    axon_gpu_context_t* ctx,
    const uint32_t* axon_indices,
    const uint32_t* segment_indices,
    const float* new_myelination,
    uint32_t count
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !axon_indices || !new_myelination || count == 0) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    // Update myelination on host side and re-upload
    // (For simplicity - could be optimized with a kernel)
    float* h_myelination = (float*)nimcp_malloc(ctx->num_axons * ctx->num_segments * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpy(h_myelination, ctx->myelination->data,
                          ctx->num_axons * ctx->num_segments * sizeof(float),
                          cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    for (uint32_t i = 0; i < count; i++) {
        uint32_t axon_idx = axon_indices[i];
        if (axon_idx >= ctx->num_axons) continue;

        if (segment_indices) {
            // Update specific segment
            uint32_t seg_idx = segment_indices[i];
            if (seg_idx < ctx->num_segments) {
                h_myelination[axon_idx * ctx->num_segments + seg_idx] = new_myelination[i];
            }
        } else {
            // Update all segments for this axon
            for (uint32_t s = 0; s < ctx->num_segments; s++) {
                h_myelination[axon_idx * ctx->num_segments + s] = new_myelination[i];
            }
        }
    }

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(ctx->myelination->data, h_myelination,
                               ctx->num_axons * ctx->num_segments * sizeof(float),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    nimcp_free(h_myelination);

    // Recalculate velocities
    return axon_gpu_update_velocities(ctx);
}

//=============================================================================
// API Implementation: Refractory and Activity
//=============================================================================

extern "C" bool axon_gpu_update_refractory(
    axon_gpu_context_t* ctx,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->num_axons == 0) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_update_refractory<<<GRID_SIZE(ctx->num_axons), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->refractory->data,
        ctx->num_axons,
        dt
    );

    return true;
}

extern "C" bool axon_gpu_get_available(
    axon_gpu_context_t* ctx,
    uint8_t* available,
    uint32_t num_axons
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !available) return false;
    if (num_axons > ctx->num_axons) num_axons = ctx->num_axons;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    uint8_t* d_available;
    NIMCP_CUDA_RECOVER(cudaMallocAsync(&d_available, num_axons * sizeof(uint8_t), stream), GPU_ERROR_OUT_OF_MEMORY);

    kernel_get_available<<<GRID_SIZE(num_axons), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->refractory->data,
        d_available,
        num_axons
    );

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(available, d_available, num_axons * sizeof(uint8_t),
                               cudaMemcpyDeviceToHost, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize(stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaFreeAsync(d_available, stream), GPU_ERROR_OUT_OF_MEMORY);

    return true;
}

extern "C" bool axon_gpu_update_activity(
    axon_gpu_context_t* ctx,
    float dt
) {
    // Activity tracking can be extended here
    // For now, this is a placeholder
    return true;
}

extern "C" bool axon_gpu_update_atp(
    axon_gpu_context_t* ctx,
    float dt,
    float regeneration_rate
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->num_axons == 0) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_update_atp<<<GRID_SIZE(ctx->num_axons), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->atp_levels->data,
        ctx->num_axons,
        dt,
        regeneration_rate
    );

    return true;
}

//=============================================================================
// API Implementation: Batch Simulation Step
//=============================================================================

extern "C" bool axon_gpu_step(
    axon_gpu_context_t* ctx,
    float dt,
    uint64_t current_time
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || ctx->num_axons == 0) return false;

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_axon_step<<<GRID_SIZE(ctx->num_axons), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->signals->data,
        (float*)ctx->positions->data,
        (float*)ctx->seg_velocities->data,
        (float*)ctx->seg_lengths->data,
        (uint8_t*)ctx->active->data,
        (float*)ctx->refractory->data,
        (float*)ctx->atp_levels->data,
        ctx->num_axons,
        ctx->num_segments,
        dt,
        ATP_REGEN_RATE
    );

    ctx->total_updates++;
    return true;
}

//=============================================================================
// API Implementation: Data Retrieval
//=============================================================================

extern "C" bool axon_gpu_get_velocities(
    axon_gpu_context_t* ctx,
    float* velocities,
    uint32_t num_axons
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !velocities) return false;
    if (num_axons > ctx->num_axons) num_axons = ctx->num_axons;

    NIMCP_CUDA_RECOVER(cudaMemcpy(velocities, ctx->velocities->data,
                          num_axons * sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool axon_gpu_get_delays(
    axon_gpu_context_t* ctx,
    float* delays,
    uint32_t num_axons
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !delays) return false;
    if (num_axons > ctx->num_axons) num_axons = ctx->num_axons;

    // Calculate total delay from segment delays (last segment cumulative)
    float* h_seg_delays = (float*)nimcp_malloc(num_axons * ctx->num_segments * sizeof(float));
    NIMCP_CUDA_RECOVER(cudaMemcpy(h_seg_delays, ctx->seg_delays->data,
                          num_axons * ctx->num_segments * sizeof(float),
                          cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);

    for (uint32_t a = 0; a < num_axons; a++) {
        delays[a] = h_seg_delays[a * ctx->num_segments + ctx->num_segments - 1];
    }

    nimcp_free(h_seg_delays);
    return true;
}

extern "C" bool axon_gpu_get_myelination(
    axon_gpu_context_t* ctx,
    float* myelination,
    uint32_t num_axons
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !myelination) return false;
    if (num_axons > ctx->num_axons) num_axons = ctx->num_axons;

    NIMCP_CUDA_RECOVER(cudaMemcpy(myelination, ctx->myelination->data,
                          num_axons * ctx->num_segments * sizeof(float),
                          cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool axon_gpu_get_signals(
    axon_gpu_context_t* ctx,
    float* signals,
    uint32_t num_axons
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !signals) return false;
    if (num_axons > ctx->num_axons) num_axons = ctx->num_axons;

    NIMCP_CUDA_RECOVER(cudaMemcpy(signals, ctx->signals->data,
                          num_axons * ctx->num_segments * sizeof(float),
                          cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Statistics
//=============================================================================

extern "C" bool axon_gpu_get_stats(
    const axon_gpu_context_t* ctx,
    axon_gpu_stats_t* stats
) {
    if (!ctx || !stats) return false;

    memset(stats, 0, sizeof(axon_gpu_stats_t));

    stats->total_spikes = ctx->total_spikes;
    stats->total_updates = ctx->total_updates;
    stats->avg_propagate_time_us = ctx->avg_propagation_time_us;
    stats->avg_velocity_time_us = ctx->avg_velocity_time_us;

    // Calculate means from device data
    if (ctx->num_axons > 0) {
        float* h_velocities = (float*)nimcp_malloc(ctx->num_axons * sizeof(float));
        float* h_myelination = (float*)nimcp_malloc(ctx->num_axons * ctx->num_segments * sizeof(float));

        cudaMemcpy(h_velocities, ctx->velocities->data,
                   ctx->num_axons * sizeof(float), cudaMemcpyDeviceToHost);
        cudaMemcpy(h_myelination, ctx->myelination->data,
                   ctx->num_axons * ctx->num_segments * sizeof(float), cudaMemcpyDeviceToHost);

        float sum_vel = 0.0f, sum_myelin = 0.0f;
        for (uint32_t a = 0; a < ctx->num_axons; a++) {
            sum_vel += h_velocities[a];
            for (uint32_t s = 0; s < ctx->num_segments; s++) {
                sum_myelin += h_myelination[a * ctx->num_segments + s];
            }
        }

        stats->mean_velocity = sum_vel / ctx->num_axons;
        stats->mean_myelination = sum_myelin / (ctx->num_axons * ctx->num_segments);

        nimcp_free(h_velocities);
        nimcp_free(h_myelination);
    }

    return true;
}

extern "C" void axon_gpu_reset_stats(axon_gpu_context_t* ctx) {
    if (!ctx) return;

    ctx->total_spikes = 0;
    ctx->total_updates = 0;
    ctx->avg_propagation_time_us = 0.0f;
    ctx->avg_velocity_time_us = 0.0f;
}

//=============================================================================
// CPU Reference Implementations
//=============================================================================

extern "C" bool axon_cpu_propagate(
    float* signals,
    const float* seg_velocities,
    uint32_t num_axons,
    uint32_t num_segments,
    float dt
) {
    if (!signals || !seg_velocities) return false;

    for (uint32_t a = 0; a < num_axons; a++) {
        uint32_t base_idx = a * num_segments;

        for (uint32_t seg = 0; seg < num_segments; seg++) {
            uint32_t idx = base_idx + seg;

            if (signals[idx] > 0.0f) {
                // Simple propagation: decay signal as it moves
                signals[idx] *= 0.99f;  // Small decay per step

                // Check if signal should move to next segment
                // (Simplified - full version would track position)
            }
        }
    }

    return true;
}

extern "C" bool axon_cpu_calculate_velocities(
    float* velocities,
    const float* diameters,
    const float* myelination,
    uint32_t num_axons,
    float base_velocity,
    float myelin_multiplier
) {
    if (!velocities || !diameters || !myelination) return false;

    for (uint32_t a = 0; a < num_axons; a++) {
        float diameter = diameters[a];
        float myelin = myelination[a];

        float velocity;
        if (myelin > 0.1f) {
            velocity = VELOCITY_COEFF_MYELINATED * diameter *
                       (0.1f + 0.9f * myelin) * myelin_multiplier / 50.0f;
        } else {
            velocity = VELOCITY_COEFF_UNMYELINATED * sqrtf(diameter);
        }

        velocity = fmaxf(velocity, MIN_VELOCITY);
        velocity = fminf(velocity, MAX_VELOCITY);

        velocities[a] = velocity;
    }

    return true;
}

extern "C" bool axon_cpu_apply_myelination(
    float* velocities,
    const float* myelination,
    uint32_t num_axons,
    float myelin_multiplier,
    float max_velocity
) {
    if (!velocities || !myelination) return false;

    for (uint32_t a = 0; a < num_axons; a++) {
        float myelin_factor = 1.0f + (myelin_multiplier - 1.0f) * myelination[a];
        velocities[a] *= myelin_factor;
        velocities[a] = fminf(velocities[a], max_velocity);
    }

    return true;
}

#endif /* NIMCP_ENABLE_CUDA */
