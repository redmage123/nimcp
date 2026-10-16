/**
 * @file nimcp_substrate_kernels.cu
 * @brief CUDA Kernels for Neural Substrate Operations
 *
 * WHAT: GPU-accelerated kernels for axon, dendrite, myelin, neuromodulator, glial ops
 * WHY:  Unified substrate processing via kernel backend strategy pattern
 * HOW:  Tensor-based operations with automatic GPU/CPU backend selection
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifdef NIMCP_ENABLE_CUDA

#include <cuda_runtime.h>
#include <math.h>

#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/recovery/nimcp_gpu_recovery.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "gpu/common/nimcp_cuda_utils.h"

#define BLOCK_SIZE 256
#define GRID_SIZE(n) (((n) + BLOCK_SIZE - 1) / BLOCK_SIZE)

// Use recovery-integrated CUDA check for kernel error checks
#define CUDA_CHECK_KERNEL_RECOVER(call, error_cat) do { \
    cudaError_t _err = (call); \
    if (_err != cudaSuccess) { \
        nimcp_gpu_recovery_result_t _result = {0}; \
        if (nimcp_gpu_try_recover(NULL, (error_cat), _err, &_result)) { \
            _err = (call); \
        } \
        if (_err != cudaSuccess) { \
            const char* _err_str = cudaGetErrorString(_err); \
            fprintf(stderr, "[NIMCP CUDA UNRECOVERABLE] %s:%d: %s returned %s\n", \
                    __FILE__, __LINE__, #call, _err_str); \
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_GPU, "CUDA kernel error (unrecoverable): %s - %s", #call, _err_str); \
            return NIMCP_KERNEL_ERROR_DEVICE; \
        } \
    } \
} while(0)

//=============================================================================
// Axon Kernels
//=============================================================================

__global__ void kernel_axon_propagate(
    const float* __restrict__ input_signals,
    const float* __restrict__ velocities,
    const float* __restrict__ myelination,
    const float* __restrict__ lengths,
    float* __restrict__ output_signals,
    float* __restrict__ delays,
    uint32_t n_axons,
    float dt
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_axons) return;

    float signal = input_signals[idx];
    float velocity = velocities[idx];
    float myelin = myelination[idx];
    float length = lengths[idx];

    // Myelination increases velocity (up to 100x for fully myelinated)
    float effective_velocity = velocity * (1.0f + 99.0f * myelin);

    // Compute propagation delay
    float delay = length / effective_velocity;
    delays[idx] = delay;

    // Signal attenuation over distance (less for myelinated axons)
    float attenuation = expf(-length / (1000.0f * (1.0f + 9.0f * myelin)));
    output_signals[idx] = signal * attenuation;
}

__global__ void kernel_axon_refractory(
    float* __restrict__ refractory_state,
    const float* __restrict__ spikes,
    uint32_t n_axons,
    float refractory_period,
    float dt
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_axons) return;

    float state = refractory_state[idx];

    // Decay refractory state
    state = fmaxf(0.0f, state - dt);

    // Reset if spike occurred
    if (spikes[idx] > 0.5f) {
        state = refractory_period;
    }

    refractory_state[idx] = state;
}

//=============================================================================
// Dendrite Kernels
//=============================================================================

__global__ void kernel_dendrite_cable_integrate(
    const float* __restrict__ inputs,
    const float* __restrict__ cable_Rm,
    const float* __restrict__ cable_Cm,
    const float* __restrict__ cable_Ra,
    float* __restrict__ voltages,
    uint32_t n_dendrites,
    uint32_t n_segments,
    float dt
) {
    uint32_t dend_idx = blockIdx.x;
    uint32_t seg_idx = threadIdx.x;

    if (dend_idx >= n_dendrites || seg_idx >= n_segments) return;

    uint32_t idx = dend_idx * n_segments + seg_idx;

    float Rm = cable_Rm[dend_idx];
    float Cm = cable_Cm[dend_idx];
    float Ra = cable_Ra[dend_idx];

    float tau = Rm * Cm;  // Membrane time constant
    float lambda = sqrtf(Rm / Ra);  // Space constant

    float V = voltages[idx];
    float I = inputs[idx];

    // Get neighboring voltages for cable equation
    float V_prev = (seg_idx > 0) ? voltages[idx - 1] : V;
    float V_next = (seg_idx < n_segments - 1) ? voltages[idx + 1] : V;

    // Cable equation: dV/dt = (V_rest - V + lambda^2 * d2V/dx2 + Rm*I) / tau
    float d2V = V_prev - 2.0f * V + V_next;
    float dV = (-V + lambda * lambda * d2V + Rm * I) / tau;

    voltages[idx] = V + dV * dt;
}

__global__ void kernel_dendrite_nmda(
    const float* __restrict__ voltages,
    const float* __restrict__ mg_block,
    float* __restrict__ nmda_current,
    float* __restrict__ nmda_spikes,
    uint32_t n_dendrites,
    uint32_t n_segments,
    float nmda_threshold
) {
    uint32_t dend_idx = blockIdx.x;
    uint32_t seg_idx = threadIdx.x;

    if (dend_idx >= n_dendrites || seg_idx >= n_segments) return;

    uint32_t idx = dend_idx * n_segments + seg_idx;

    float V = voltages[idx];
    float mg = mg_block[dend_idx];

    // Mg2+ block relief is voltage-dependent
    float mg_relief = 1.0f / (1.0f + mg * expf(-0.062f * V));

    // NMDA current
    float I_nmda = mg_relief * fmaxf(0.0f, V + 70.0f);
    nmda_current[idx] = I_nmda;

    // Detect NMDA spike (using atomicMax for dendrite-wide detection)
    if (I_nmda > nmda_threshold) {
        atomicMax((int*)&nmda_spikes[dend_idx], __float_as_int(1.0f));
    }
}

__global__ void kernel_dendrite_calcium(
    const float* __restrict__ nmda_current,
    const float* __restrict__ vgcc_current,
    float* __restrict__ calcium,
    float* __restrict__ calcium_decay,
    uint32_t n_spines,
    float tau_calcium,
    float dt
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_spines) return;

    float Ca = calcium[idx];
    float decay_state = calcium_decay[idx];

    // Calcium influx from NMDA and VGCC
    float influx = nmda_current[idx] * 0.1f + vgcc_current[idx] * 0.05f;

    // Calcium dynamics with decay
    float dCa = influx - Ca / tau_calcium;
    Ca = fmaxf(0.0f, Ca + dCa * dt);

    calcium[idx] = Ca;
    calcium_decay[idx] = Ca / tau_calcium;
}

__global__ void kernel_dendrite_bap(
    const float* __restrict__ soma_spike,
    const float* __restrict__ attenuation,
    float* __restrict__ bap_signal,
    uint32_t n_dendrites,
    uint32_t n_segments,
    float bap_velocity,
    float dt
) {
    uint32_t dend_idx = blockIdx.x;
    uint32_t seg_idx = threadIdx.x;

    if (dend_idx >= n_dendrites || seg_idx >= n_segments) return;

    uint32_t idx = dend_idx * n_segments + seg_idx;

    float spike = soma_spike[dend_idx];
    float atten = attenuation[idx];

    // bAP propagates from soma with distance-dependent attenuation
    float bap = spike * atten;

    // Temporal decay
    float current_bap = bap_signal[idx];
    bap_signal[idx] = fmaxf(bap, current_bap * expf(-dt / 2.0f));
}

//=============================================================================
// Myelin Kernels
//=============================================================================

__global__ void kernel_myelin_g_ratio(
    const float* __restrict__ axon_diameter,
    const float* __restrict__ fiber_diameter,
    float* __restrict__ g_ratio,
    float* __restrict__ is_optimal,
    uint32_t n_axons
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_axons) return;

    float d_axon = axon_diameter[idx];
    float d_fiber = fiber_diameter[idx];

    // G-ratio = axon diameter / fiber diameter
    float g = (d_fiber > 0.0f) ? d_axon / d_fiber : 0.0f;
    g_ratio[idx] = g;

    // Optimal G-ratio is ~0.6-0.7 (Rushton's theory)
    float optimal = 0.6f;
    float tolerance = 0.1f;
    is_optimal[idx] = (fabsf(g - optimal) < tolerance) ? 1.0f : 0.0f;
}

__global__ void kernel_myelin_conduction_velocity(
    const float* __restrict__ g_ratio,
    const float* __restrict__ internode_length,
    const float* __restrict__ temperature,
    float* __restrict__ velocity,
    uint32_t n_axons,
    bool scalar_temp
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_axons) return;

    float g = g_ratio[idx];
    float L = internode_length[idx];
    float T = scalar_temp ? temperature[0] : temperature[idx];

    // Velocity scales with fiber diameter and temperature
    // V = k * d * Q10^((T-37)/10) where Q10 ~ 1.5
    float Q10 = 1.5f;
    float temp_factor = powf(Q10, (T - 37.0f) / 10.0f);

    // Optimal velocity when g-ratio ~ 0.6
    float g_factor = 4.0f * g * (1.0f - g);  // Peaks at g=0.5

    // Base velocity ~ 100 m/s for large myelinated fibers
    float base_velocity = 100.0f * L / 1000.0f;  // Scale with internode length

    velocity[idx] = base_velocity * g_factor * temp_factor;
}

__global__ void kernel_myelin_plasticity(
    const float* __restrict__ activity,
    const float* __restrict__ oligodendrocyte_signal,
    float* __restrict__ myelin_thickness,
    float* __restrict__ sheath_length,
    uint32_t n_axons,
    float learning_rate,
    float dt
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_axons) return;

    float act = activity[idx];
    float ol_signal = oligodendrocyte_signal[idx];

    float thickness = myelin_thickness[idx];
    float length = sheath_length[idx];

    // Activity-dependent myelination
    float drive = act * ol_signal;

    // Thickness increases with activity (saturates at max)
    float max_thickness = 2.0f;  // um
    float d_thickness = learning_rate * drive * (max_thickness - thickness);
    thickness = fminf(max_thickness, thickness + d_thickness * dt);

    // Sheath length also adapts
    float max_length = 200.0f;  // um
    float d_length = learning_rate * 0.5f * drive * (max_length - length);
    length = fminf(max_length, length + d_length * dt);

    myelin_thickness[idx] = thickness;
    sheath_length[idx] = length;
}

//=============================================================================
// Neuromodulator Kernels
//=============================================================================

__global__ void kernel_neuromod_decay(
    float* __restrict__ concentrations,
    const float* __restrict__ decay_rates,
    uint32_t n_pools,
    uint32_t n_types,
    float dt
) {
    uint32_t pool_idx = blockIdx.x;
    uint32_t type_idx = threadIdx.x;

    if (pool_idx >= n_pools || type_idx >= n_types) return;

    uint32_t idx = pool_idx * n_types + type_idx;

    float conc = concentrations[idx];
    float decay = decay_rates[type_idx];

    // Exponential decay: C(t+dt) = C(t) * exp(-dt/tau)
    concentrations[idx] = conc * expf(-dt * decay);
}

__global__ void kernel_neuromod_release(
    const uint32_t* __restrict__ release_sites,
    const uint32_t* __restrict__ release_types,
    const float* __restrict__ release_amounts,
    float* __restrict__ concentrations,
    uint32_t n_events,
    uint32_t n_types
) {
    uint32_t event_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (event_idx >= n_events) return;

    uint32_t site = release_sites[event_idx];
    uint32_t type = release_types[event_idx];
    float amount = release_amounts[event_idx];

    // Atomic add to handle concurrent releases
    uint32_t idx = site * n_types + type;
    atomicAdd(&concentrations[idx], amount);
}

__global__ void kernel_neuromod_effect(
    const float* __restrict__ concentrations,
    const float* __restrict__ receptor_density,
    float* __restrict__ modulation,
    uint32_t n_synapses,
    uint32_t n_types,
    uint32_t n_pools
) {
    uint32_t syn_idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (syn_idx >= n_synapses) return;

    float mod = 1.0f;

    // Accumulate modulation from all neuromodulator types
    for (uint32_t t = 0; t < n_types; t++) {
        float density = receptor_density[syn_idx * n_types + t];

        // Average concentration across pools (simplified)
        float avg_conc = 0.0f;
        for (uint32_t p = 0; p < n_pools; p++) {
            avg_conc += concentrations[p * n_types + t];
        }
        avg_conc /= (float)n_pools;

        // Receptor occupancy -> modulation
        float occupancy = avg_conc / (avg_conc + 0.5f);  // Michaelis-Menten
        mod *= (1.0f + density * occupancy);
    }

    modulation[syn_idx] = mod;
}

__global__ void kernel_neuromod_phasic_tonic(
    const float* __restrict__ phasic_input,
    float* __restrict__ tonic_level,
    float* __restrict__ total_level,
    uint32_t n_pools,
    uint32_t n_types,
    float tonic_tau,
    float phasic_decay,
    float dt
) {
    uint32_t pool_idx = blockIdx.x;
    uint32_t type_idx = threadIdx.x;

    if (pool_idx >= n_pools || type_idx >= n_types) return;

    uint32_t idx = pool_idx * n_types + type_idx;

    float phasic = phasic_input[idx];
    float tonic = tonic_level[idx];

    // Tonic level slowly tracks phasic average
    float tonic_target = 0.3f;  // Baseline tonic level
    tonic = tonic + (tonic_target - tonic) * dt / tonic_tau;

    // Phasic bursts add on top
    float phasic_contribution = phasic * expf(-dt * phasic_decay);

    tonic_level[idx] = tonic;
    total_level[idx] = tonic + phasic_contribution;
}

//=============================================================================
// Glial Cell Kernels
//=============================================================================

__global__ void kernel_astrocyte_calcium_wave(
    const float* __restrict__ ip3_levels,
    const float* __restrict__ gap_junctions,
    float* __restrict__ calcium,
    float* __restrict__ wave_front,
    uint32_t n_astro,
    uint32_t n_neighbors,
    float diffusion_rate,
    float dt
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_astro) return;

    float Ca = calcium[idx];
    float ip3 = ip3_levels[idx];
    float wave = wave_front[idx];

    // IP3-induced calcium release
    float release = ip3 * (1.0f - Ca) * 0.1f;

    // Diffusion from neighbors through gap junctions
    float diffusion = 0.0f;
    for (uint32_t n = 0; n < n_neighbors; n++) {
        uint32_t neighbor_idx = (uint32_t)gap_junctions[idx * n_neighbors + n];
        if (neighbor_idx < n_astro && neighbor_idx != idx) {
            float neighbor_Ca = calcium[neighbor_idx];
            float coupling = gap_junctions[idx * n_neighbors + n];
            diffusion += coupling * (neighbor_Ca - Ca);
        }
    }

    // Calcium dynamics
    float decay = Ca * 0.05f;
    float dCa = release + diffusion_rate * diffusion - decay;
    Ca = fmaxf(0.0f, fminf(1.0f, Ca + dCa * dt));

    // Wave front propagation
    wave = (Ca > 0.5f) ? 1.0f : wave * 0.9f;

    calcium[idx] = Ca;
    wave_front[idx] = wave;
}

__global__ void kernel_astrocyte_release(
    const float* __restrict__ calcium,
    const float* __restrict__ threshold,
    float* __restrict__ glutamate_release,
    float* __restrict__ atp_release,
    uint32_t n_astro
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_astro) return;

    float Ca = calcium[idx];
    float thresh = threshold[idx];

    // Threshold-based vesicle release
    if (Ca > thresh) {
        float excess = Ca - thresh;
        glutamate_release[idx] = excess * 0.5f;
        atp_release[idx] = excess * 0.3f;
    } else {
        glutamate_release[idx] = 0.0f;
        atp_release[idx] = 0.0f;
    }
}

__global__ void kernel_microglia_activation(
    const float* __restrict__ damage_signals,
    const float* __restrict__ anti_inflam,
    float* __restrict__ activation_state,
    float* __restrict__ phagocytic_activity,
    uint32_t n_micro,
    float activation_threshold,
    float dt
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_micro) return;

    float damage = damage_signals[idx];
    float anti = anti_inflam[idx];
    float state = activation_state[idx];

    // M0 (resting) -> M1 (pro-inflammatory) -> M2 (anti-inflammatory)
    // State: 0 = M0, 1 = M1, 2 = M2

    float pro_drive = damage - activation_threshold;
    float anti_drive = anti - 0.5f;

    if (pro_drive > 0 && state < 1.0f) {
        state = fminf(1.0f, state + pro_drive * dt);
    } else if (anti_drive > 0 && state > 0.0f) {
        state = fminf(2.0f, fmaxf(0.0f, state + anti_drive * dt));
    } else {
        // Return to resting
        state = state * (1.0f - 0.1f * dt);
    }

    // Phagocytic activity peaks in M1 state
    float phago = (state > 0.5f && state < 1.5f) ? (1.0f - fabsf(state - 1.0f)) : 0.0f;

    activation_state[idx] = state;
    phagocytic_activity[idx] = phago;
}

__global__ void kernel_oligodendrocyte_differentiation(
    const float* __restrict__ activity_signal,
    const float* __restrict__ growth_factors,
    float* __restrict__ differentiation_state,
    float* __restrict__ myelin_production,
    uint32_t n_opc,
    float dt
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_opc) return;

    float activity = activity_signal[idx];
    float growth = growth_factors[idx];
    float state = differentiation_state[idx];

    // OPC -> pre-OL -> OL differentiation (state 0 -> 1)
    float diff_drive = activity * growth;
    float d_state = diff_drive * 0.01f * (1.0f - state);
    state = fminf(1.0f, state + d_state * dt);

    // Myelin production increases as differentiation progresses
    float production = state * state * growth;

    differentiation_state[idx] = state;
    myelin_production[idx] = production;
}

//=============================================================================
// Metabolic Kernels
//=============================================================================

__global__ void kernel_metabolic_effects(
    const float* __restrict__ atp_levels,
    const float* __restrict__ oxygen_levels,
    const float* __restrict__ glucose_levels,
    float* __restrict__ capacity,
    float* __restrict__ fatigue,
    uint32_t n_regions
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_regions) return;

    float atp = atp_levels[idx];
    float o2 = oxygen_levels[idx];
    float glucose = glucose_levels[idx];

    // Metabolic capacity is limited by the scarcest resource
    float cap = fminf(atp, fminf(o2, glucose));
    capacity[idx] = cap;

    // Fatigue accumulates when capacity is low
    float fat = (cap < 0.5f) ? (0.5f - cap) * 2.0f : 0.0f;
    fatigue[idx] = fat;
}

__global__ void kernel_metabolic_update(
    const float* __restrict__ neural_activity,
    float* __restrict__ atp_levels,
    float* __restrict__ lactate_levels,
    uint32_t n_regions,
    float consumption_rate,
    float recovery_rate,
    float dt
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n_regions) return;

    float activity = neural_activity[idx];
    float atp = atp_levels[idx];
    float lactate = lactate_levels[idx];

    // ATP consumption proportional to activity
    float consumption = activity * consumption_rate * dt;
    atp = fmaxf(0.0f, atp - consumption);

    // ATP recovery (anaerobic produces lactate)
    float recovery = recovery_rate * (1.0f - atp) * dt;
    atp = fminf(1.0f, atp + recovery);

    // Lactate production during high activity
    float lactate_prod = consumption * 0.3f;
    float lactate_clear = lactate * 0.1f * dt;
    lactate = fmaxf(0.0f, lactate + lactate_prod - lactate_clear);

    atp_levels[idx] = atp;
    lactate_levels[idx] = lactate;
}

//=============================================================================
// C API Wrappers for Kernel Backend
//=============================================================================

extern "C" {

// Helper to get tensor data pointer
static inline float* tensor_data(nimcp_gpu_tensor_t* t) {
    return t ? (float*)t->data : nullptr;
}

static inline const float* tensor_data_const(const nimcp_gpu_tensor_t* t) {
    return t ? (const float*)t->data : nullptr;
}

static inline uint32_t tensor_size(const nimcp_gpu_tensor_t* t) {
    if (!t) return 0;
    uint32_t size = 1;
    for (uint32_t i = 0; i < t->ndim; i++) {
        size *= t->dims[i];
    }
    return size;
}

//=============================================================================
// Axon Operations
//=============================================================================

nimcp_kernel_error_t cuda_axon_propagate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input_signals,
    const nimcp_gpu_tensor_t* velocities,
    const nimcp_gpu_tensor_t* myelination,
    const nimcp_gpu_tensor_t* lengths,
    nimcp_gpu_tensor_t* output_signals,
    nimcp_gpu_tensor_t* delays,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !input_signals || !velocities || !myelination ||
        !lengths || !output_signals || !delays) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(input_signals);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_axon_propagate<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(input_signals),
        tensor_data_const(velocities),
        tensor_data_const(myelination),
        tensor_data_const(lengths),
        tensor_data(output_signals),
        tensor_data(delays),
        n, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_axon_refractory(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* refractory_state,
    const nimcp_gpu_tensor_t* spikes,
    float refractory_period,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !refractory_state || !spikes) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(refractory_state);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_axon_refractory<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data(refractory_state),
        tensor_data_const(spikes),
        n, refractory_period, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Dendrite Operations
//=============================================================================

nimcp_kernel_error_t cuda_dendrite_cable_integrate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* inputs,
    const nimcp_gpu_tensor_t* cable_Rm,
    const nimcp_gpu_tensor_t* cable_Cm,
    const nimcp_gpu_tensor_t* cable_Ra,
    nimcp_gpu_tensor_t* voltages,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !inputs || !cable_Rm || !cable_Cm || !cable_Ra || !voltages) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (voltages->ndim < 2) return NIMCP_KERNEL_ERROR_INVALID_SIZE;

    uint32_t n_dend = voltages->dims[0];
    uint32_t n_seg = voltages->dims[1];

    dim3 grid(n_dend);
    dim3 block(n_seg);

    kernel_dendrite_cable_integrate<<<grid, block>>>(
        tensor_data_const(inputs),
        tensor_data_const(cable_Rm),
        tensor_data_const(cable_Cm),
        tensor_data_const(cable_Ra),
        tensor_data(voltages),
        n_dend, n_seg, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_dendrite_nmda(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* voltages,
    const nimcp_gpu_tensor_t* mg_block,
    nimcp_gpu_tensor_t* nmda_current,
    nimcp_gpu_tensor_t* nmda_spikes,
    float nmda_threshold
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !voltages || !mg_block || !nmda_current || !nmda_spikes) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (voltages->ndim < 2) return NIMCP_KERNEL_ERROR_INVALID_SIZE;

    uint32_t n_dend = voltages->dims[0];
    uint32_t n_seg = voltages->dims[1];

    // Zero spike detection array
    cudaMemset(tensor_data(nmda_spikes), 0, n_dend * sizeof(float));

    dim3 grid(n_dend);
    dim3 block(n_seg);

    kernel_dendrite_nmda<<<grid, block>>>(
        tensor_data_const(voltages),
        tensor_data_const(mg_block),
        tensor_data(nmda_current),
        tensor_data(nmda_spikes),
        n_dend, n_seg, nmda_threshold
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_dendrite_calcium(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* nmda_current,
    const nimcp_gpu_tensor_t* vgcc_current,
    nimcp_gpu_tensor_t* calcium,
    nimcp_gpu_tensor_t* calcium_decay,
    float tau_calcium,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !nmda_current || !vgcc_current || !calcium || !calcium_decay) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(calcium);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_dendrite_calcium<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(nmda_current),
        tensor_data_const(vgcc_current),
        tensor_data(calcium),
        tensor_data(calcium_decay),
        n, tau_calcium, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_dendrite_bap(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* soma_spike,
    const nimcp_gpu_tensor_t* attenuation,
    nimcp_gpu_tensor_t* bap_signal,
    float bap_velocity,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !soma_spike || !attenuation || !bap_signal) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (bap_signal->ndim < 2) return NIMCP_KERNEL_ERROR_INVALID_SIZE;

    uint32_t n_dend = bap_signal->dims[0];
    uint32_t n_seg = bap_signal->dims[1];

    dim3 grid(n_dend);
    dim3 block(n_seg);

    kernel_dendrite_bap<<<grid, block>>>(
        tensor_data_const(soma_spike),
        tensor_data_const(attenuation),
        tensor_data(bap_signal),
        n_dend, n_seg, bap_velocity, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Myelin Operations
//=============================================================================

nimcp_kernel_error_t cuda_myelin_g_ratio(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* axon_diameter,
    const nimcp_gpu_tensor_t* fiber_diameter,
    nimcp_gpu_tensor_t* g_ratio,
    nimcp_gpu_tensor_t* is_optimal
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !axon_diameter || !fiber_diameter || !g_ratio || !is_optimal) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(axon_diameter);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_myelin_g_ratio<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(axon_diameter),
        tensor_data_const(fiber_diameter),
        tensor_data(g_ratio),
        tensor_data(is_optimal),
        n
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_myelin_conduction_velocity(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* g_ratio,
    const nimcp_gpu_tensor_t* internode_length,
    const nimcp_gpu_tensor_t* temperature,
    nimcp_gpu_tensor_t* velocity
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !g_ratio || !internode_length || !temperature || !velocity) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(g_ratio);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    bool scalar_temp = (tensor_size(temperature) == 1);

    kernel_myelin_conduction_velocity<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(g_ratio),
        tensor_data_const(internode_length),
        tensor_data_const(temperature),
        tensor_data(velocity),
        n, scalar_temp
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_myelin_plasticity(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity,
    const nimcp_gpu_tensor_t* oligodendrocyte_signal,
    nimcp_gpu_tensor_t* myelin_thickness,
    nimcp_gpu_tensor_t* sheath_length,
    float learning_rate,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !activity || !oligodendrocyte_signal ||
        !myelin_thickness || !sheath_length) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(activity);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_myelin_plasticity<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(activity),
        tensor_data_const(oligodendrocyte_signal),
        tensor_data(myelin_thickness),
        tensor_data(sheath_length),
        n, learning_rate, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Neuromodulator Operations
//=============================================================================

nimcp_kernel_error_t cuda_neuromod_decay(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* concentrations,
    const nimcp_gpu_tensor_t* decay_rates,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !concentrations || !decay_rates) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (concentrations->ndim < 2) return NIMCP_KERNEL_ERROR_INVALID_SIZE;

    uint32_t n_pools = concentrations->dims[0];
    uint32_t n_types = concentrations->dims[1];

    dim3 grid(n_pools);
    dim3 block(n_types);

    kernel_neuromod_decay<<<grid, block>>>(
        tensor_data(concentrations),
        tensor_data_const(decay_rates),
        n_pools, n_types, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_neuromod_release(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* release_sites,
    const nimcp_gpu_tensor_t* release_types,
    const nimcp_gpu_tensor_t* release_amounts,
    nimcp_gpu_tensor_t* concentrations,
    uint32_t n_events
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !release_sites || !release_types ||
        !release_amounts || !concentrations) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (n_events == 0) return NIMCP_KERNEL_SUCCESS;

    uint32_t n_types = concentrations->dims[1];

    kernel_neuromod_release<<<GRID_SIZE(n_events), BLOCK_SIZE>>>(
        (const uint32_t*)tensor_data_const(release_sites),
        (const uint32_t*)tensor_data_const(release_types),
        tensor_data_const(release_amounts),
        tensor_data(concentrations),
        n_events, n_types
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_neuromod_effect(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* concentrations,
    const nimcp_gpu_tensor_t* receptor_density,
    nimcp_gpu_tensor_t* modulation
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !concentrations || !receptor_density || !modulation) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n_synapses = tensor_size(modulation);
    if (n_synapses == 0) return NIMCP_KERNEL_SUCCESS;

    uint32_t n_types = concentrations->dims[1];
    uint32_t n_pools = concentrations->dims[0];

    kernel_neuromod_effect<<<GRID_SIZE(n_synapses), BLOCK_SIZE>>>(
        tensor_data_const(concentrations),
        tensor_data_const(receptor_density),
        tensor_data(modulation),
        n_synapses, n_types, n_pools
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_neuromod_phasic_tonic(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phasic_input,
    nimcp_gpu_tensor_t* tonic_level,
    nimcp_gpu_tensor_t* total_level,
    float tonic_tau,
    float phasic_decay,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !phasic_input || !tonic_level || !total_level) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (phasic_input->ndim < 2) return NIMCP_KERNEL_ERROR_INVALID_SIZE;

    uint32_t n_pools = phasic_input->dims[0];
    uint32_t n_types = phasic_input->dims[1];

    dim3 grid(n_pools);
    dim3 block(n_types);

    kernel_neuromod_phasic_tonic<<<grid, block>>>(
        tensor_data_const(phasic_input),
        tensor_data(tonic_level),
        tensor_data(total_level),
        n_pools, n_types, tonic_tau, phasic_decay, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Glial Operations
//=============================================================================

nimcp_kernel_error_t cuda_astrocyte_calcium_wave(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* ip3_levels,
    const nimcp_gpu_tensor_t* gap_junctions,
    nimcp_gpu_tensor_t* calcium,
    nimcp_gpu_tensor_t* wave_front,
    float diffusion_rate,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !ip3_levels || !gap_junctions || !calcium || !wave_front) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n_astro = tensor_size(calcium);
    if (n_astro == 0) return NIMCP_KERNEL_SUCCESS;

    uint32_t n_neighbors = gap_junctions->dims[1];

    kernel_astrocyte_calcium_wave<<<GRID_SIZE(n_astro), BLOCK_SIZE>>>(
        tensor_data_const(ip3_levels),
        tensor_data_const(gap_junctions),
        tensor_data(calcium),
        tensor_data(wave_front),
        n_astro, n_neighbors, diffusion_rate, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_astrocyte_release(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* calcium,
    const nimcp_gpu_tensor_t* threshold,
    nimcp_gpu_tensor_t* glutamate_release,
    nimcp_gpu_tensor_t* atp_release
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !calcium || !threshold || !glutamate_release || !atp_release) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(calcium);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_astrocyte_release<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(calcium),
        tensor_data_const(threshold),
        tensor_data(glutamate_release),
        tensor_data(atp_release),
        n
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_microglia_activation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* damage_signals,
    const nimcp_gpu_tensor_t* anti_inflam,
    nimcp_gpu_tensor_t* activation_state,
    nimcp_gpu_tensor_t* phagocytic_activity,
    float activation_threshold,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !damage_signals || !anti_inflam ||
        !activation_state || !phagocytic_activity) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(damage_signals);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_microglia_activation<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(damage_signals),
        tensor_data_const(anti_inflam),
        tensor_data(activation_state),
        tensor_data(phagocytic_activity),
        n, activation_threshold, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_oligodendrocyte_differentiation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity_signal,
    const nimcp_gpu_tensor_t* growth_factors,
    nimcp_gpu_tensor_t* differentiation_state,
    nimcp_gpu_tensor_t* myelin_production,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !activity_signal || !growth_factors ||
        !differentiation_state || !myelin_production) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(activity_signal);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_oligodendrocyte_differentiation<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(activity_signal),
        tensor_data_const(growth_factors),
        tensor_data(differentiation_state),
        tensor_data(myelin_production),
        n, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Metabolic Operations
//=============================================================================

nimcp_kernel_error_t cuda_metabolic_effects(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* atp_levels,
    const nimcp_gpu_tensor_t* oxygen_levels,
    const nimcp_gpu_tensor_t* glucose_levels,
    nimcp_gpu_tensor_t* capacity,
    nimcp_gpu_tensor_t* fatigue
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !atp_levels || !oxygen_levels || !glucose_levels ||
        !capacity || !fatigue) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(atp_levels);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_metabolic_effects<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(atp_levels),
        tensor_data_const(oxygen_levels),
        tensor_data_const(glucose_levels),
        tensor_data(capacity),
        tensor_data(fatigue),
        n
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cuda_metabolic_update(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* neural_activity,
    nimcp_gpu_tensor_t* atp_levels,
    nimcp_gpu_tensor_t* lactate_levels,
    float consumption_rate,
    float recovery_rate,
    float dt
) {
    if (!nimcp_gpu_recovery_is_initialized()) {
        nimcp_gpu_recovery_init(NULL);
    }

    if (!ctx || !neural_activity || !atp_levels || !lactate_levels) {
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(neural_activity);
    if (n == 0) return NIMCP_KERNEL_SUCCESS;

    kernel_metabolic_update<<<GRID_SIZE(n), BLOCK_SIZE>>>(
        tensor_data_const(neural_activity),
        tensor_data(atp_levels),
        tensor_data(lactate_levels),
        n, consumption_rate, recovery_rate, dt
    );

    CUDA_CHECK_KERNEL_RECOVER(cudaGetLastError(), GPU_ERROR_KERNEL_LAUNCH);
    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CUDA Substrate Ops Table
//=============================================================================

nimcp_substrate_ops_t nimcp_cuda_substrate_ops = {
    // Axon
    .axon_propagate = cuda_axon_propagate,
    .axon_refractory = cuda_axon_refractory,

    // Dendrite
    .dendrite_cable_integrate = cuda_dendrite_cable_integrate,
    .dendrite_nmda = cuda_dendrite_nmda,
    .dendrite_calcium = cuda_dendrite_calcium,
    .dendrite_bap = cuda_dendrite_bap,

    // Myelin
    .myelin_g_ratio = cuda_myelin_g_ratio,
    .myelin_conduction_velocity = cuda_myelin_conduction_velocity,
    .myelin_plasticity = cuda_myelin_plasticity,

    // Neuromodulator
    .neuromod_decay = cuda_neuromod_decay,
    .neuromod_release = cuda_neuromod_release,
    .neuromod_effect = cuda_neuromod_effect,
    .neuromod_phasic_tonic = cuda_neuromod_phasic_tonic,

    // Glial
    .astrocyte_calcium_wave = cuda_astrocyte_calcium_wave,
    .astrocyte_release = cuda_astrocyte_release,
    .microglia_activation = cuda_microglia_activation,
    .oligodendrocyte_differentiation = cuda_oligodendrocyte_differentiation,

    // Metabolic
    .metabolic_effects = cuda_metabolic_effects,
    .metabolic_update = cuda_metabolic_update
};

} // extern "C"

#endif // NIMCP_ENABLE_CUDA
