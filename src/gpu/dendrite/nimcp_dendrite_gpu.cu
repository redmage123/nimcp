/**
 * @file nimcp_dendrite_gpu.cu
 * @brief GPU CUDA Kernels for Dendrite Module
 *
 * WHAT: CUDA kernels for GPU-accelerated dendritic computation
 * WHY:  GPU acceleration for parallel cable equation, calcium dynamics, STDP
 * HOW:  Custom kernels using nimcp_gpu_tensor_t for tensor-based operations
 *
 * BIOLOGICAL BASIS:
 * =================
 * Dendrites perform sophisticated computations requiring parallel processing:
 * - Cable equation: dV/dt = (I_syn - V/R_m + I_axial) / C_m
 * - Calcium dynamics: dCa/dt = -Ca/tau + influx
 * - NMDA spikes: Voltage-dependent Mg2+ unblock
 * - STDP: Timing-dependent weight updates
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * - Cable equation: 1 thread per segment (independent integration)
 * - Calcium: 1 thread per spine (independent decay/influx)
 * - NMDA: 1 thread per segment (parallel threshold detection)
 * - STDP: 1 thread per spine (independent weight update)
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include "utils/memory/nimcp_memory.h"
#include <cuda_runtime.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "gpu/dendrite/nimcp_dendrite_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"
#include "constants/nimcp_math_constants.h"

#define LOG_MODULE "DENDRITE_GPU"

#define BLOCK_SIZE 256
#define WARP_SIZE 32
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

// Biological constants
#define NMDA_MG_BLOCK_CONSTANT 0.062f      // Mg2+ block voltage sensitivity (1/mV)
#define NMDA_MG_CONCENTRATION 1.0f         // External [Mg2+] (mM)
#define CALCIUM_INFLUX_FACTOR 0.1f         // Ca2+ influx per mV
#define BAP_CALCIUM_FACTOR 0.5f            // Ca2+ from bAP

//=============================================================================
// CUDA Kernels: Cable Equation Integration
//=============================================================================

/**
 * @brief Cable equation integration kernel
 *
 * Each thread integrates one segment:
 * dV/dt = (I_leak + I_axial) / C_m
 * I_leak = -V / R_m
 *
 * @param voltages       [num_dendrites, num_segments] - membrane potentials
 * @param cable_params   [num_dendrites, 3] - Rm, Cm, Ra per dendrite
 * @param axial_currents [num_dendrites, num_segments] - axial currents
 * @param num_dendrites  Number of dendrites
 * @param num_segments   Segments per dendrite
 * @param dt_ms          Time step
 */
__global__ void kernel_dendrite_integrate(
    float* __restrict__ voltages,
    const float* __restrict__ cable_params,
    const float* __restrict__ axial_currents,
    uint32_t num_dendrites,
    uint32_t num_segments,
    float dt_ms
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total_segments = num_dendrites * num_segments;
    if (idx >= total_segments) return;

    // Compute dendrite and segment indices
    uint32_t dendrite_idx = idx / num_segments;
    uint32_t segment_idx = idx % num_segments;

    // Get cable parameters for this dendrite (Rm, Cm, Ra)
    float rm = cable_params[dendrite_idx * 3 + 0];  // Membrane resistance
    float cm = cable_params[dendrite_idx * 3 + 1];  // Membrane capacitance
    // float ra = cable_params[dendrite_idx * 3 + 2];  // Axial resistance (used in axial_currents)

    // Current voltage
    float V = voltages[idx];

    // Leak current: I_leak = -V / R_m
    float I_leak = -V / rm;

    // Axial current from neighbors (precomputed)
    float I_axial = axial_currents[idx];

    // Total current
    float I_total = I_leak + I_axial;

    // Voltage update: dV = I * dt / C_m
    float dV = (I_total / cm) * (dt_ms / 1000.0f);  // Convert ms to s

    // Update voltage
    V += dV;

    // Clamp voltage to physiological range
    V = fmaxf(-80.0f, fminf(40.0f, V));  // -80mV to +40mV

    voltages[idx] = V;
}

/**
 * @brief Compute axial currents between segments
 *
 * Each thread computes axial current for one segment:
 * I_axial = g_axial * (V_parent - V_segment) + sum(g_child * (V_child - V_segment))
 *
 * @param axial_currents  [num_dendrites, num_segments] - output currents
 * @param voltages        [num_dendrites, num_segments] - membrane potentials
 * @param segment_parents [num_dendrites, num_segments] - parent segment indices
 * @param segment_lengths [num_dendrites, num_segments] - segment lengths
 * @param segment_diameters [num_dendrites, num_segments] - segment diameters
 * @param cable_params    [num_dendrites, 3] - Rm, Cm, Ra
 * @param num_dendrites   Number of dendrites
 * @param num_segments    Segments per dendrite
 */
__global__ void kernel_compute_axial_currents(
    float* __restrict__ axial_currents,
    const float* __restrict__ voltages,
    const uint32_t* __restrict__ segment_parents,
    const float* __restrict__ segment_lengths,
    const float* __restrict__ segment_diameters,
    const float* __restrict__ cable_params,
    uint32_t num_dendrites,
    uint32_t num_segments
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total_segments = num_dendrites * num_segments;
    if (idx >= total_segments) return;

    uint32_t dendrite_idx = idx / num_segments;
    uint32_t segment_idx = idx % num_segments;
    uint32_t base_idx = dendrite_idx * num_segments;

    float ra = cable_params[dendrite_idx * 3 + 2];  // Axial resistance
    float V_segment = voltages[idx];
    float length = segment_lengths[idx];
    float diameter = segment_diameters[idx];

    // Calculate axial conductance: g_axial = pi * d^2 / (4 * Ra * L)
    float radius = diameter / 2.0f;
    float g_axial = (NIMCP_PI_F * radius * radius) / (ra * length) * 1e6f;  // Convert to nS

    float I_total = 0.0f;

    // Current from parent
    uint32_t parent_idx = segment_parents[idx];
    if (parent_idx != 0xFFFFFFFF && parent_idx < num_segments) {
        float V_parent = voltages[base_idx + parent_idx];
        I_total += g_axial * (V_parent - V_segment);
    }

    // Current from children (search for segments with this segment as parent)
    for (uint32_t c = 0; c < num_segments; c++) {
        if (segment_parents[base_idx + c] == segment_idx) {
            float V_child = voltages[base_idx + c];
            float child_length = segment_lengths[base_idx + c];
            float child_diameter = segment_diameters[base_idx + c];
            float child_radius = child_diameter / 2.0f;
            float child_g = (NIMCP_PI_F * child_radius * child_radius) / (ra * child_length) * 1e6f;
            I_total += child_g * (V_child - V_segment);
        }
    }

    axial_currents[idx] = I_total;
}

//=============================================================================
// CUDA Kernels: Calcium Dynamics
//=============================================================================

/**
 * @brief Calcium dynamics update kernel
 *
 * Each thread updates one spine's calcium:
 * dCa/dt = -Ca/tau + influx_nmda + influx_bap
 *
 * @param calcium       [num_dendrites, num_spines] - calcium concentrations
 * @param nmda_states   [num_dendrites, num_segments] - NMDA activation
 * @param bap_signals   [num_dendrites, num_segments] - bAP amplitudes
 * @param spine_segments [num_dendrites, num_spines] - segment index for each spine
 * @param num_dendrites Number of dendrites
 * @param num_spines    Spines per dendrite
 * @param num_segments  Segments per dendrite
 * @param dt_ms         Time step
 * @param decay_tau_ms  Calcium decay time constant
 */
__global__ void kernel_dendrite_calcium(
    float* __restrict__ calcium,
    const float* __restrict__ nmda_states,
    const float* __restrict__ bap_signals,
    const uint32_t* __restrict__ spine_segments,
    uint32_t num_dendrites,
    uint32_t num_spines,
    uint32_t num_segments,
    float dt_ms,
    float decay_tau_ms
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total_spines = num_dendrites * num_spines;
    if (idx >= total_spines) return;

    uint32_t dendrite_idx = idx / num_spines;
    uint32_t spine_idx = idx % num_spines;

    // Get segment index for this spine
    uint32_t segment_idx = spine_segments[idx];
    uint32_t segment_flat_idx = dendrite_idx * num_segments + segment_idx;

    // Current calcium
    float Ca = calcium[idx];

    // Exponential decay
    float decay_factor = expf(-dt_ms / decay_tau_ms);
    Ca *= decay_factor;

    // NMDA-mediated calcium influx
    float nmda = nmda_states[segment_flat_idx];
    if (nmda > 0.0f) {
        Ca += nmda * CALCIUM_INFLUX_FACTOR * dt_ms;
    }

    // bAP-mediated calcium influx
    float bap = bap_signals[segment_flat_idx];
    if (bap > 0.0f) {
        Ca += bap * BAP_CALCIUM_FACTOR * dt_ms;
    }

    // Clamp calcium
    Ca = fmaxf(0.0f, fminf(100.0f, Ca));  // 0-100 uM

    calcium[idx] = Ca;
}

//=============================================================================
// CUDA Kernels: NMDA Spike Detection
//=============================================================================

/**
 * @brief Calculate Mg2+ block factor (device function)
 *
 * B(V) = 1 / (1 + [Mg]_o * exp(-0.062*V))
 */
__device__ float calculate_mg_block(float voltage_mv) {
    float block = 1.0f / (1.0f + NMDA_MG_CONCENTRATION *
                          expf(-NMDA_MG_BLOCK_CONSTANT * voltage_mv));
    return block;
}

/**
 * @brief NMDA spike detection kernel
 *
 * Each thread checks one segment for NMDA spike conditions:
 * - Voltage above threshold
 * - Mg2+ block sufficiently relieved
 *
 * @param nmda_states    [num_dendrites, num_segments] - NMDA activation (output)
 * @param voltages       [num_dendrites, num_segments] - membrane potentials
 * @param segment_active [num_dendrites, num_segments] - has active properties
 * @param num_dendrites  Number of dendrites
 * @param num_segments   Segments per dendrite
 * @param threshold_mv   NMDA spike threshold
 */
__global__ void kernel_dendrite_nmda(
    float* __restrict__ nmda_states,
    const float* __restrict__ voltages,
    const uint32_t* __restrict__ segment_active,
    uint32_t num_dendrites,
    uint32_t num_segments,
    float threshold_mv
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t total_segments = num_dendrites * num_segments;
    if (idx >= total_segments) return;

    float V = voltages[idx];
    uint32_t has_active = segment_active[idx];

    // Default: no NMDA spike
    float nmda = 0.0f;

    // Check for NMDA spike conditions
    if (has_active && V > threshold_mv) {
        // Calculate Mg2+ block relief
        float mg_block = calculate_mg_block(V);

        // NMDA spike if Mg2+ block is sufficiently relieved
        if (mg_block > 0.5f) {
            // NMDA spike amplitude proportional to voltage and Mg2+ unblock
            nmda = (V - threshold_mv) * mg_block;

            // Clamp to maximum
            nmda = fminf(nmda, 40.0f);  // Max 40mV NMDA spike
        }
    } else {
        // Decay existing NMDA state
        nmda = nmda_states[idx] * 0.95f;  // Decay factor
    }

    nmda_states[idx] = nmda;
}

//=============================================================================
// CUDA Kernels: STDP Weight Updates
//=============================================================================

/**
 * @brief STDP weight update kernel
 *
 * Each thread processes one STDP event and updates:
 * - Pre/post eligibility traces
 * - Synaptic weight based on timing difference
 *
 * @param weights      [num_dendrites, num_spines] - synaptic weights
 * @param pre_traces   [num_dendrites, num_spines] - presynaptic traces
 * @param post_traces  [num_dendrites, num_spines] - postsynaptic traces
 * @param events       Array of STDP events
 * @param num_events   Number of events
 * @param num_spines   Spines per dendrite
 * @param a_plus       LTP amplitude
 * @param a_minus      LTD amplitude
 * @param tau_plus_ms  LTP time constant
 * @param tau_minus_ms LTD time constant
 * @param current_time Current simulation time (us)
 */
__global__ void kernel_dendrite_stdp(
    float* __restrict__ weights,
    float* __restrict__ pre_traces,
    float* __restrict__ post_traces,
    const dendrite_gpu_stdp_event_t* __restrict__ events,
    uint32_t num_events,
    uint32_t num_spines,
    float a_plus,
    float a_minus,
    float tau_plus_ms,
    float tau_minus_ms,
    uint64_t current_time
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_events) return;

    dendrite_gpu_stdp_event_t event = events[idx];
    uint32_t spine_flat_idx = event.dendrite_id * num_spines + event.spine_id;

    // Get current traces
    float pre_trace = pre_traces[spine_flat_idx];
    float post_trace = post_traces[spine_flat_idx];
    float weight = weights[spine_flat_idx];

    // Compute timing difference (ms)
    float dt_ms = (float)((int64_t)event.post_time - (int64_t)event.pre_time) / 1000.0f;

    float dW = 0.0f;

    if (dt_ms > 0.0f) {
        // Post after pre -> LTP
        float ltp = a_plus * expf(-dt_ms / tau_plus_ms) * pre_trace;
        dW += ltp;

        // Update post trace
        post_trace = fminf(1.0f, post_trace + 1.0f);
    } else if (dt_ms < 0.0f) {
        // Pre after post -> LTD
        float ltd = a_minus * expf(dt_ms / tau_minus_ms) * post_trace;  // dt_ms < 0
        dW -= ltd;

        // Update pre trace
        pre_trace = fminf(1.0f, pre_trace + 1.0f);
    }

    // Apply weight change
    weight += dW;

    // Bound weights
    weight = fmaxf(0.0f, fminf(1.0f, weight));

    // Write back
    weights[spine_flat_idx] = weight;
    pre_traces[spine_flat_idx] = pre_trace;
    post_traces[spine_flat_idx] = post_trace;
}

/**
 * @brief Decay STDP traces kernel
 *
 * Each thread decays traces for one spine:
 * trace *= exp(-dt/tau)
 */
__global__ void kernel_decay_traces(
    float* __restrict__ pre_traces,
    float* __restrict__ post_traces,
    uint32_t total_spines,
    float dt_ms,
    float tau_plus_ms,
    float tau_minus_ms
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_spines) return;

    pre_traces[idx] *= expf(-dt_ms / tau_plus_ms);
    post_traces[idx] *= expf(-dt_ms / tau_minus_ms);
}

//=============================================================================
// CUDA Kernels: bAP Propagation
//=============================================================================

/**
 * @brief bAP propagation kernel
 *
 * Each thread updates bAP signal for one segment based on distance
 *
 * @param bap_signals     [num_dendrites, num_segments] - bAP amplitudes
 * @param segment_distances [num_dendrites, num_segments] - path distances from soma
 * @param num_segments    Segments per dendrite
 * @param total_segments  Total number of segments
 * @param amplitude_mv    Initial AP amplitude at soma
 * @param attenuation     Attenuation per micrometer
 * @param velocity        bAP velocity (um/ms)
 * @param dt_ms           Time step
 */
__global__ void kernel_propagate_bap(
    float* __restrict__ bap_signals,
    const float* __restrict__ segment_distances,
    uint32_t num_segments,
    uint32_t total_segments,
    float amplitude_mv,
    float attenuation_per_um,
    float velocity_um_ms,
    float dt_ms
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_segments) return;

    float distance = segment_distances[idx];

    // Calculate bAP amplitude at this distance
    float attenuation = expf(-attenuation_per_um * distance);
    float bap = amplitude_mv * attenuation;

    // Decay existing signal
    float current_bap = bap_signals[idx];
    current_bap *= 0.9f;  // Decay factor per step

    // Add new bAP if it reaches this segment
    float time_to_reach = distance / velocity_um_ms;
    if (time_to_reach < dt_ms) {
        current_bap = fmaxf(current_bap, bap);
    }

    bap_signals[idx] = current_bap;
}

//=============================================================================
// CUDA Kernels: Current Injection
//=============================================================================

/**
 * @brief Inject synaptic currents into spines
 *
 * Each thread processes one input event
 */
__global__ void kernel_inject_currents(
    float* __restrict__ calcium,
    float* __restrict__ pre_traces,
    const uint32_t* __restrict__ spine_indices,
    const uint32_t* __restrict__ dendrite_indices,
    const float* __restrict__ currents,
    uint32_t num_inputs,
    uint32_t num_spines
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= num_inputs) return;

    uint32_t spine_idx = spine_indices[idx];
    uint32_t dendrite_idx = dendrite_indices[idx];
    float current = currents[idx];

    uint32_t flat_idx = dendrite_idx * num_spines + spine_idx;

    // Add calcium influx proportional to current
    atomicAdd(&calcium[flat_idx], current * 0.01f);

    // Update pre-trace for STDP
    atomicAdd(&pre_traces[flat_idx], 0.1f);
}

//=============================================================================
// API Implementation: Configuration
//=============================================================================

extern "C" dendrite_gpu_config_t dendrite_gpu_default_config(void) {
    dendrite_gpu_config_t config;
    config.max_dendrites = 1024;
    config.max_segments_per_dendrite = 64;
    config.max_spines_per_dendrite = 256;
    config.default_rm = 20000.0f;  // Ohm*cm^2
    config.default_cm = 1.0f;       // uF/cm^2
    config.default_ra = 150.0f;     // Ohm*cm
    config.calcium_decay_tau_ms = 20.0f;
    config.nmda_threshold_mv = 20.0f;
    config.bap_velocity_um_ms = 500.0f;
    config.bap_attenuation_per_um = 0.003f;
    config.stdp_tau_plus_ms = 17.0f;
    config.stdp_tau_minus_ms = 34.0f;
    config.stdp_a_plus = 0.005f;
    config.stdp_a_minus = 0.00525f;
    config.enable_async_transfer = true;
    return config;
}

//=============================================================================
// API Implementation: Lifecycle
//=============================================================================

extern "C" dendrite_gpu_context_t* dendrite_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const dendrite_gpu_config_t* config
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!gpu_ctx) {
        LOG_ERROR("NULL GPU context");
        return NULL;
    }

    dendrite_gpu_context_t* ctx = (dendrite_gpu_context_t*)nimcp_calloc(1, sizeof(dendrite_gpu_context_t));
    if (!ctx) {
        LOG_ERROR("Failed to allocate dendrite GPU context");
        return NULL;
    }

    ctx->gpu_ctx = gpu_ctx;
    ctx->config = config ? *config : dendrite_gpu_default_config();

    // Create CUDA stream
    cudaError_t err = cudaStreamCreate((cudaStream_t*)&ctx->stream);
    if (err != cudaSuccess) {
        LOG_ERROR("Failed to create CUDA stream: %s", cudaGetErrorString(err));
        nimcp_free(ctx);
        return NULL;
    }

    // Pre-allocate dimensions
    uint32_t max_d = ctx->config.max_dendrites;
    uint32_t max_seg = ctx->config.max_segments_per_dendrite;
    uint32_t max_spine = ctx->config.max_spines_per_dendrite;

    size_t dims_seg[2] = {max_d, max_seg};
    size_t dims_spine[2] = {max_d, max_spine};
    size_t dims_cable[2] = {max_d, 3};

    // Create tensors for segment data
    ctx->segment_voltages = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->nmda_states = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->bap_signals = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->axial_currents = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->segment_lengths = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->segment_diameters = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->segment_distances = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->segment_parents = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_UINT32);
    ctx->segment_active = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_UINT32);

    // Create tensors for spine data
    ctx->calcium_levels = nimcp_gpu_tensor_create(gpu_ctx, dims_spine, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->spine_weights = nimcp_gpu_tensor_create(gpu_ctx, dims_spine, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->spine_segments = nimcp_gpu_tensor_create(gpu_ctx, dims_spine, 2, NIMCP_GPU_PRECISION_UINT32);
    ctx->spine_resistances = nimcp_gpu_tensor_create(gpu_ctx, dims_spine, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->spine_capacitances = nimcp_gpu_tensor_create(gpu_ctx, dims_spine, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->pre_traces = nimcp_gpu_tensor_create(gpu_ctx, dims_spine, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->post_traces = nimcp_gpu_tensor_create(gpu_ctx, dims_spine, 2, NIMCP_GPU_PRECISION_FP32);

    // Create cable parameters tensor
    ctx->cable_params = nimcp_gpu_tensor_create(gpu_ctx, dims_cable, 2, NIMCP_GPU_PRECISION_FP32);

    // Create temp buffers
    ctx->temp_buffer_1 = nimcp_gpu_tensor_create(gpu_ctx, dims_seg, 2, NIMCP_GPU_PRECISION_FP32);
    ctx->temp_buffer_2 = nimcp_gpu_tensor_create(gpu_ctx, dims_spine, 2, NIMCP_GPU_PRECISION_FP32);

    // Check all allocations
    if (!ctx->segment_voltages || !ctx->calcium_levels || !ctx->spine_weights ||
        !ctx->nmda_states || !ctx->bap_signals || !ctx->cable_params ||
        !ctx->segment_lengths || !ctx->segment_diameters || !ctx->segment_distances ||
        !ctx->segment_parents || !ctx->segment_active || !ctx->spine_segments ||
        !ctx->pre_traces || !ctx->post_traces || !ctx->axial_currents) {
        LOG_ERROR("Failed to allocate GPU tensors");
        dendrite_gpu_destroy(ctx);
        return NULL;
    }

    // Initialize to zero
    nimcp_gpu_zeros(gpu_ctx, ctx->segment_voltages);
    nimcp_gpu_zeros(gpu_ctx, ctx->calcium_levels);
    nimcp_gpu_zeros(gpu_ctx, ctx->nmda_states);
    nimcp_gpu_zeros(gpu_ctx, ctx->bap_signals);
    nimcp_gpu_zeros(gpu_ctx, ctx->axial_currents);
    nimcp_gpu_zeros(gpu_ctx, ctx->pre_traces);
    nimcp_gpu_zeros(gpu_ctx, ctx->post_traces);

    // Initialize weights to 1.0
    nimcp_gpu_ones(gpu_ctx, ctx->spine_weights);

    LOG_INFO("GPU Dendrite context created (dendrites: %u, segments: %u, spines: %u)",
             max_d, max_seg, max_spine);

    return ctx;
}

extern "C" void dendrite_gpu_destroy(dendrite_gpu_context_t* ctx) {
    if (!ctx) return;

    // Synchronize before cleanup
    if (ctx->stream) {
        cudaStreamSynchronize((cudaStream_t)ctx->stream);
        cudaStreamDestroy((cudaStream_t)ctx->stream);
    }

    // Destroy tensors
    if (ctx->segment_voltages) nimcp_gpu_tensor_destroy(ctx->segment_voltages);
    if (ctx->calcium_levels) nimcp_gpu_tensor_destroy(ctx->calcium_levels);
    if (ctx->spine_weights) nimcp_gpu_tensor_destroy(ctx->spine_weights);
    if (ctx->nmda_states) nimcp_gpu_tensor_destroy(ctx->nmda_states);
    if (ctx->bap_signals) nimcp_gpu_tensor_destroy(ctx->bap_signals);
    if (ctx->cable_params) nimcp_gpu_tensor_destroy(ctx->cable_params);
    if (ctx->segment_lengths) nimcp_gpu_tensor_destroy(ctx->segment_lengths);
    if (ctx->segment_diameters) nimcp_gpu_tensor_destroy(ctx->segment_diameters);
    if (ctx->segment_distances) nimcp_gpu_tensor_destroy(ctx->segment_distances);
    if (ctx->segment_parents) nimcp_gpu_tensor_destroy(ctx->segment_parents);
    if (ctx->segment_active) nimcp_gpu_tensor_destroy(ctx->segment_active);
    if (ctx->spine_segments) nimcp_gpu_tensor_destroy(ctx->spine_segments);
    if (ctx->spine_resistances) nimcp_gpu_tensor_destroy(ctx->spine_resistances);
    if (ctx->spine_capacitances) nimcp_gpu_tensor_destroy(ctx->spine_capacitances);
    if (ctx->pre_traces) nimcp_gpu_tensor_destroy(ctx->pre_traces);
    if (ctx->post_traces) nimcp_gpu_tensor_destroy(ctx->post_traces);
    if (ctx->axial_currents) nimcp_gpu_tensor_destroy(ctx->axial_currents);
    if (ctx->temp_buffer_1) nimcp_gpu_tensor_destroy(ctx->temp_buffer_1);
    if (ctx->temp_buffer_2) nimcp_gpu_tensor_destroy(ctx->temp_buffer_2);

    nimcp_free(ctx);
    LOG_DEBUG("GPU Dendrite context destroyed");
}

extern "C" bool dendrite_gpu_synchronize(dendrite_gpu_context_t* ctx) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;
    NIMCP_CUDA_RECOVER(cudaStreamSynchronize((cudaStream_t)ctx->stream), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Data Upload
//=============================================================================

extern "C" bool dendrite_gpu_upload_segments(
    dendrite_gpu_context_t* ctx,
    const dendrite_gpu_segment_t* segments,
    uint32_t num_dendrites,
    uint32_t segments_per_dendrite
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !segments) return false;
    if (num_dendrites > ctx->config.max_dendrites ||
        segments_per_dendrite > ctx->config.max_segments_per_dendrite) {
        LOG_ERROR("Segment count exceeds configuration");
        return false;
    }

    uint32_t total = num_dendrites * segments_per_dendrite;

    // Allocate host arrays
    float* h_voltages = (float*)nimcp_malloc(total * sizeof(float));
    float* h_lengths = (float*)nimcp_malloc(total * sizeof(float));
    float* h_diameters = (float*)nimcp_malloc(total * sizeof(float));
    float* h_distances = (float*)nimcp_malloc(total * sizeof(float));
    uint32_t* h_parents = (uint32_t*)nimcp_malloc(total * sizeof(uint32_t));
    uint32_t* h_active = (uint32_t*)nimcp_malloc(total * sizeof(uint32_t));
    float* h_cable = (float*)nimcp_malloc(num_dendrites * 3 * sizeof(float));

    if (!h_voltages || !h_lengths || !h_diameters || !h_distances ||
        !h_parents || !h_active || !h_cable) {
        nimcp_free(h_voltages); nimcp_free(h_lengths); nimcp_free(h_diameters);
        nimcp_free(h_distances); nimcp_free(h_parents); nimcp_free(h_active); nimcp_free(h_cable);
        return false;
    }

    // Extract data from segment structures
    for (uint32_t i = 0; i < total; i++) {
        h_voltages[i] = segments[i].voltage;
        h_lengths[i] = segments[i].length;
        h_diameters[i] = segments[i].diameter;
        h_distances[i] = segments[i].path_distance;
        h_parents[i] = segments[i].parent_segment;
        h_active[i] = segments[i].has_active_properties;
    }

    // Extract cable params (assume first segment of each dendrite has them)
    for (uint32_t d = 0; d < num_dendrites; d++) {
        uint32_t base = d * segments_per_dendrite;
        h_cable[d * 3 + 0] = segments[base].rm;
        h_cable[d * 3 + 1] = segments[base].cm;
        h_cable[d * 3 + 2] = segments[base].ra;
    }

    // Upload to GPU tensors
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->segment_voltages->data, h_voltages,
                          total * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->segment_lengths->data, h_lengths,
                          total * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->segment_diameters->data, h_diameters,
                          total * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->segment_distances->data, h_distances,
                          total * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->segment_parents->data, h_parents,
                          total * sizeof(uint32_t), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->segment_active->data, h_active,
                          total * sizeof(uint32_t), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->cable_params->data, h_cable,
                          num_dendrites * 3 * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    ctx->num_dendrites = num_dendrites;
    ctx->num_segments = segments_per_dendrite;

    nimcp_free(h_voltages); nimcp_free(h_lengths); nimcp_free(h_diameters);
    nimcp_free(h_distances); nimcp_free(h_parents); nimcp_free(h_active); nimcp_free(h_cable);

    LOG_DEBUG("Uploaded %u segments for %u dendrites", total, num_dendrites);
    return true;
}

extern "C" bool dendrite_gpu_upload_spines(
    dendrite_gpu_context_t* ctx,
    const dendrite_gpu_spine_t* spines,
    uint32_t num_dendrites,
    uint32_t spines_per_dendrite
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !spines) return false;
    if (num_dendrites > ctx->config.max_dendrites ||
        spines_per_dendrite > ctx->config.max_spines_per_dendrite) {
        LOG_ERROR("Spine count exceeds configuration");
        return false;
    }

    uint32_t total = num_dendrites * spines_per_dendrite;

    // Allocate host arrays
    float* h_calcium = (float*)nimcp_malloc(total * sizeof(float));
    float* h_weights = (float*)nimcp_malloc(total * sizeof(float));
    uint32_t* h_segments = (uint32_t*)nimcp_malloc(total * sizeof(uint32_t));
    float* h_pre = (float*)nimcp_malloc(total * sizeof(float));
    float* h_post = (float*)nimcp_malloc(total * sizeof(float));

    if (!h_calcium || !h_weights || !h_segments || !h_pre || !h_post) {
        nimcp_free(h_calcium); nimcp_free(h_weights); nimcp_free(h_segments);
        nimcp_free(h_pre); nimcp_free(h_post);
        return false;
    }

    // Extract data
    for (uint32_t i = 0; i < total; i++) {
        h_calcium[i] = spines[i].calcium;
        h_weights[i] = spines[i].synaptic_weight;
        h_segments[i] = spines[i].segment_id;
        h_pre[i] = spines[i].pre_trace;
        h_post[i] = spines[i].post_trace;
    }

    // Upload to GPU
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->calcium_levels->data, h_calcium,
                          total * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->spine_weights->data, h_weights,
                          total * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->spine_segments->data, h_segments,
                          total * sizeof(uint32_t), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->pre_traces->data, h_pre,
                          total * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->post_traces->data, h_post,
                          total * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    ctx->num_spines = spines_per_dendrite;

    nimcp_free(h_calcium); nimcp_free(h_weights); nimcp_free(h_segments);
    nimcp_free(h_pre); nimcp_free(h_post);

    LOG_DEBUG("Uploaded %u spines for %u dendrites", total, num_dendrites);
    return true;
}

extern "C" bool dendrite_gpu_upload_cable_params(
    dendrite_gpu_context_t* ctx,
    const float* rm,
    const float* cm,
    const float* ra,
    uint32_t num_dendrites
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !rm || !cm || !ra) return false;

    float* h_cable = (float*)nimcp_malloc(num_dendrites * 3 * sizeof(float));
    if (!h_cable) return false;

    for (uint32_t d = 0; d < num_dendrites; d++) {
        h_cable[d * 3 + 0] = rm[d];
        h_cable[d * 3 + 1] = cm[d];
        h_cable[d * 3 + 2] = ra[d];
    }

    NIMCP_CUDA_RECOVER(cudaMemcpy(ctx->cable_params->data, h_cable,
                          num_dendrites * 3 * sizeof(float), cudaMemcpyHostToDevice), GPU_ERROR_CUDA_RUNTIME);

    nimcp_free(h_cable);
    return true;
}

//=============================================================================
// API Implementation: Download
//=============================================================================

extern "C" bool dendrite_gpu_download_voltages(
    dendrite_gpu_context_t* ctx,
    float* voltages
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !voltages) return false;
    uint32_t total = ctx->num_dendrites * ctx->num_segments;
    NIMCP_CUDA_RECOVER(cudaMemcpy(voltages, ctx->segment_voltages->data,
                          total * sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool dendrite_gpu_download_calcium(
    dendrite_gpu_context_t* ctx,
    float* calcium
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !calcium) return false;
    uint32_t total = ctx->num_dendrites * ctx->num_spines;
    NIMCP_CUDA_RECOVER(cudaMemcpy(calcium, ctx->calcium_levels->data,
                          total * sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool dendrite_gpu_download_weights(
    dendrite_gpu_context_t* ctx,
    float* weights
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !weights) return false;
    uint32_t total = ctx->num_dendrites * ctx->num_spines;
    NIMCP_CUDA_RECOVER(cudaMemcpy(weights, ctx->spine_weights->data,
                          total * sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

extern "C" bool dendrite_gpu_download_nmda_states(
    dendrite_gpu_context_t* ctx,
    float* nmda_states
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nmda_states) return false;
    uint32_t total = ctx->num_dendrites * ctx->num_segments;
    NIMCP_CUDA_RECOVER(cudaMemcpy(nmda_states, ctx->nmda_states->data,
                          total * sizeof(float), cudaMemcpyDeviceToHost), GPU_ERROR_CUDA_RUNTIME);
    return true;
}

//=============================================================================
// API Implementation: Core Computation
//=============================================================================

extern "C" bool dendrite_gpu_integrate(
    dendrite_gpu_context_t* ctx,
    float dt_ms
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;

    uint32_t total_segments = ctx->num_dendrites * ctx->num_segments;
    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_dendrite_integrate<<<GRID_SIZE(total_segments), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->segment_voltages->data,
        (const float*)ctx->cable_params->data,
        (const float*)ctx->axial_currents->data,
        ctx->num_dendrites,
        ctx->num_segments,
        dt_ms
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    ctx->integrate_calls++;
    return true;
}

extern "C" bool dendrite_gpu_update_calcium(
    dendrite_gpu_context_t* ctx,
    float dt_ms
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;

    uint32_t total_spines = ctx->num_dendrites * ctx->num_spines;
    if (total_spines == 0) return true;  // No spines to update

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_dendrite_calcium<<<GRID_SIZE(total_spines), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->calcium_levels->data,
        (const float*)ctx->nmda_states->data,
        (const float*)ctx->bap_signals->data,
        (const uint32_t*)ctx->spine_segments->data,
        ctx->num_dendrites,
        ctx->num_spines,
        ctx->num_segments,
        dt_ms,
        ctx->config.calcium_decay_tau_ms
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    ctx->calcium_updates++;
    return true;
}

extern "C" bool dendrite_gpu_detect_nmda_spikes(
    dendrite_gpu_context_t* ctx
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;

    uint32_t total_segments = ctx->num_dendrites * ctx->num_segments;
    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_dendrite_nmda<<<GRID_SIZE(total_segments), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->nmda_states->data,
        (const float*)ctx->segment_voltages->data,
        (const uint32_t*)ctx->segment_active->data,
        ctx->num_dendrites,
        ctx->num_segments,
        ctx->config.nmda_threshold_mv
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    ctx->nmda_detections++;
    return true;
}

extern "C" bool dendrite_gpu_apply_stdp(
    dendrite_gpu_context_t* ctx,
    const dendrite_gpu_stdp_event_t* events,
    uint32_t num_events,
    uint64_t timestamp
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;
    if (!events || num_events == 0) return true;  // No-op is valid

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    // Upload events to GPU
    dendrite_gpu_stdp_event_t* d_events;
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_events, num_events * sizeof(dendrite_gpu_stdp_event_t)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_events, events,
                               num_events * sizeof(dendrite_gpu_stdp_event_t),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    kernel_dendrite_stdp<<<GRID_SIZE(num_events), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->spine_weights->data,
        (float*)ctx->pre_traces->data,
        (float*)ctx->post_traces->data,
        d_events,
        num_events,
        ctx->num_spines,
        ctx->config.stdp_a_plus,
        ctx->config.stdp_a_minus,
        ctx->config.stdp_tau_plus_ms,
        ctx->config.stdp_tau_minus_ms,
        timestamp
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    NIMCP_CUDA_RECOVER(cudaFree(d_events), GPU_ERROR_OUT_OF_MEMORY);

    ctx->stdp_updates++;
    return true;
}

extern "C" bool dendrite_gpu_propagate_bap(
    dendrite_gpu_context_t* ctx,
    float amplitude_mv,
    float dt_ms
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;

    uint32_t total_segments = ctx->num_dendrites * ctx->num_segments;
    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_propagate_bap<<<GRID_SIZE(total_segments), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->bap_signals->data,
        (const float*)ctx->segment_distances->data,
        ctx->num_segments,
        total_segments,
        amplitude_mv,
        ctx->config.bap_attenuation_per_um,
        ctx->config.bap_velocity_um_ms,
        dt_ms
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool dendrite_gpu_compute_axial_currents(
    dendrite_gpu_context_t* ctx
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;

    uint32_t total_segments = ctx->num_dendrites * ctx->num_segments;
    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_compute_axial_currents<<<GRID_SIZE(total_segments), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->axial_currents->data,
        (const float*)ctx->segment_voltages->data,
        (const uint32_t*)ctx->segment_parents->data,
        (const float*)ctx->segment_lengths->data,
        (const float*)ctx->segment_diameters->data,
        (const float*)ctx->cable_params->data,
        ctx->num_dendrites,
        ctx->num_segments
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool dendrite_gpu_inject_currents(
    dendrite_gpu_context_t* ctx,
    const uint32_t* spine_indices,
    const uint32_t* dendrite_indices,
    const float* currents,
    uint32_t num_inputs
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !spine_indices || !dendrite_indices || !currents || num_inputs == 0) {
        return false;
    }

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    // Upload inputs to GPU
    uint32_t* d_spine_indices;
    uint32_t* d_dendrite_indices;
    float* d_currents;

    NIMCP_CUDA_RECOVER(cudaMalloc(&d_spine_indices, num_inputs * sizeof(uint32_t)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_dendrite_indices, num_inputs * sizeof(uint32_t)), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaMalloc(&d_currents, num_inputs * sizeof(float)), GPU_ERROR_OUT_OF_MEMORY);

    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_spine_indices, spine_indices,
                               num_inputs * sizeof(uint32_t),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_dendrite_indices, dendrite_indices,
                               num_inputs * sizeof(uint32_t),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);
    NIMCP_CUDA_RECOVER(cudaMemcpyAsync(d_currents, currents,
                               num_inputs * sizeof(float),
                               cudaMemcpyHostToDevice, stream), GPU_ERROR_CUDA_RUNTIME);

    kernel_inject_currents<<<GRID_SIZE(num_inputs), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->calcium_levels->data,
        (float*)ctx->pre_traces->data,
        d_spine_indices,
        d_dendrite_indices,
        d_currents,
        num_inputs,
        ctx->num_spines
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);

    NIMCP_CUDA_RECOVER(cudaFree(d_spine_indices), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaFree(d_dendrite_indices), GPU_ERROR_OUT_OF_MEMORY);
    NIMCP_CUDA_RECOVER(cudaFree(d_currents), GPU_ERROR_OUT_OF_MEMORY);

    return true;
}

extern "C" bool dendrite_gpu_decay_traces(
    dendrite_gpu_context_t* ctx,
    float dt_ms
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;

    uint32_t total_spines = ctx->num_dendrites * ctx->num_spines;
    if (total_spines == 0) return true;  // No traces to decay

    cudaStream_t stream = (cudaStream_t)ctx->stream;

    kernel_decay_traces<<<GRID_SIZE(total_spines), BLOCK_SIZE, 0, stream>>>(
        (float*)ctx->pre_traces->data,
        (float*)ctx->post_traces->data,
        total_spines,
        dt_ms,
        ctx->config.stdp_tau_plus_ms,
        ctx->config.stdp_tau_minus_ms
    );

    NIMCP_CUDA_RECOVER_LAST(GPU_ERROR_KERNEL_LAUNCH);
    return true;
}

extern "C" bool dendrite_gpu_step(
    dendrite_gpu_context_t* ctx,
    float dt_ms,
    uint64_t timestamp
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx) return false;

    // 1. Compute axial currents
    if (!dendrite_gpu_compute_axial_currents(ctx)) return false;

    // 2. Integrate cable equation
    if (!dendrite_gpu_integrate(ctx, dt_ms)) return false;

    // 3. Detect NMDA spikes
    if (!dendrite_gpu_detect_nmda_spikes(ctx)) return false;

    // 4. Update calcium
    if (!dendrite_gpu_update_calcium(ctx, dt_ms)) return false;

    // 5. Decay STDP traces
    if (!dendrite_gpu_decay_traces(ctx, dt_ms)) return false;

    return true;
}

//=============================================================================
// API Implementation: Statistics
//=============================================================================

extern "C" bool dendrite_gpu_get_stats(
    const dendrite_gpu_context_t* ctx,
    dendrite_gpu_stats_t* stats
) {
    if (!ctx || !stats) return false;

    stats->integrate_calls = ctx->integrate_calls;
    stats->calcium_updates = ctx->calcium_updates;
    stats->nmda_detections = ctx->nmda_detections;
    stats->stdp_updates = ctx->stdp_updates;
    stats->avg_integrate_time_us = ctx->avg_integrate_time_us;
    stats->avg_calcium_time_us = ctx->avg_calcium_time_us;
    stats->avg_nmda_time_us = 0.0f;  // Not tracked yet
    stats->avg_stdp_time_us = 0.0f;  // Not tracked yet
    stats->gpu_memory_used = ctx->gpu_memory_used;
    stats->active_dendrites = ctx->num_dendrites;
    stats->active_segments = ctx->num_segments;
    stats->active_spines = ctx->num_spines;
    stats->mean_voltage = 0.0f;  // Would need reduction kernel
    stats->mean_calcium = 0.0f;  // Would need reduction kernel

    return true;
}

extern "C" void dendrite_gpu_reset_stats(dendrite_gpu_context_t* ctx) {
    if (ctx) {
        ctx->integrate_calls = 0;
        ctx->calcium_updates = 0;
        ctx->nmda_detections = 0;
        ctx->stdp_updates = 0;
        ctx->avg_integrate_time_us = 0.0f;
        ctx->avg_calcium_time_us = 0.0f;
    }
}

//=============================================================================
// CPU Reference Implementations (for testing)
//=============================================================================

extern "C" bool dendrite_cpu_integrate(
    float* voltages,
    const float* cable_params,
    const float* axial_currents,
    uint32_t num_dendrites,
    uint32_t num_segments,
    float dt_ms
) {
    if (!voltages || !cable_params || !axial_currents) return false;

    for (uint32_t d = 0; d < num_dendrites; d++) {
        float rm = cable_params[d * 3 + 0];
        float cm = cable_params[d * 3 + 1];

        for (uint32_t s = 0; s < num_segments; s++) {
            uint32_t idx = d * num_segments + s;

            float V = voltages[idx];
            float I_leak = -V / rm;
            float I_axial = axial_currents[idx];
            float I_total = I_leak + I_axial;
            float dV = (I_total / cm) * (dt_ms / 1000.0f);
            V += dV;
            V = fmaxf(-80.0f, fminf(40.0f, V));
            voltages[idx] = V;
        }
    }

    return true;
}

extern "C" bool dendrite_cpu_update_calcium(
    float* calcium,
    const float* nmda_states,
    const float* bap_signals,
    uint32_t num_dendrites,
    uint32_t num_spines,
    float dt_ms,
    float decay_tau_ms
) {
    if (!calcium) return false;

    float decay_factor = expf(-dt_ms / decay_tau_ms);

    for (uint32_t i = 0; i < num_dendrites * num_spines; i++) {
        float Ca = calcium[i];
        Ca *= decay_factor;

        // Note: nmda_states and bap_signals are per-segment, need mapping
        // Simplified: just decay
        Ca = fmaxf(0.0f, fminf(100.0f, Ca));
        calcium[i] = Ca;
    }

    return true;
}

extern "C" bool dendrite_cpu_detect_nmda_spikes(
    float* nmda_states,
    const float* voltages,
    uint32_t num_dendrites,
    uint32_t num_segments,
    float threshold_mv
) {
    if (!nmda_states || !voltages) return false;

    for (uint32_t i = 0; i < num_dendrites * num_segments; i++) {
        float V = voltages[i];

        if (V > threshold_mv) {
            float mg_block = 1.0f / (1.0f + NMDA_MG_CONCENTRATION *
                                     expf(-NMDA_MG_BLOCK_CONSTANT * V));
            if (mg_block > 0.5f) {
                nmda_states[i] = fminf((V - threshold_mv) * mg_block, 40.0f);
            } else {
                nmda_states[i] *= 0.95f;
            }
        } else {
            nmda_states[i] *= 0.95f;
        }
    }

    return true;
}

extern "C" bool dendrite_cpu_apply_stdp(
    float* weights,
    float* pre_traces,
    float* post_traces,
    const dendrite_gpu_stdp_event_t* events,
    uint32_t num_events,
    float a_plus,
    float a_minus,
    float tau_plus_ms,
    float tau_minus_ms
) {
    if (!weights || !pre_traces || !post_traces || !events) return false;

    for (uint32_t i = 0; i < num_events; i++) {
        // Note: This is simplified - would need num_spines for proper indexing
        float dt_ms = (float)((int64_t)events[i].post_time -
                             (int64_t)events[i].pre_time) / 1000.0f;

        float dW = 0.0f;
        if (dt_ms > 0.0f) {
            dW = a_plus * expf(-dt_ms / tau_plus_ms);
        } else if (dt_ms < 0.0f) {
            dW = -a_minus * expf(dt_ms / tau_minus_ms);
        }

        // Would need proper indexing here
        // weights[...] += dW;
    }

    return true;
}

#endif /* NIMCP_ENABLE_CUDA */
