/**
 * @file nimcp_substrate_cpu.c
 * @brief CPU Fallback Implementations for Neural Substrate Operations
 *
 * WHAT: CPU implementations of substrate ops for systems without GPU
 * WHY:  Ensures substrate operations work on any hardware
 * HOW:  Sequential implementations mirroring CUDA kernel logic
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#include "gpu/backend/nimcp_kernel_backend.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <math.h>
#include <string.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(substrate_cpu)

// Helper to get tensor data
static inline float* tensor_data(nimcp_gpu_tensor_t* t) {
    return t ? (float*)t->data : NULL;
}

static inline const float* tensor_data_const(const nimcp_gpu_tensor_t* t) {
    return t ? (const float*)t->data : NULL;
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
// Axon Operations (CPU)
//=============================================================================

nimcp_kernel_error_t cpu_axon_propagate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input_signals,
    const nimcp_gpu_tensor_t* velocities,
    const nimcp_gpu_tensor_t* myelination,
    const nimcp_gpu_tensor_t* lengths,
    nimcp_gpu_tensor_t* output_signals,
    nimcp_gpu_tensor_t* delays,
    float dt
) {
    (void)ctx; (void)dt;

    if (!input_signals || !velocities || !myelination ||
        !lengths || !output_signals || !delays) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_axon_propagate: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(input_signals);
    const float* in_sig = tensor_data_const(input_signals);
    const float* vel = tensor_data_const(velocities);
    const float* myel = tensor_data_const(myelination);
    const float* len = tensor_data_const(lengths);
    float* out_sig = tensor_data(output_signals);
    float* del = tensor_data(delays);

    for (uint32_t i = 0; i < n; i++) {
        float effective_velocity = vel[i] * (1.0f + 99.0f * myel[i]);
        del[i] = len[i] / effective_velocity;
        float attenuation = expf(-len[i] / (1000.0f * (1.0f + 9.0f * myel[i])));
        out_sig[i] = in_sig[i] * attenuation;
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_axon_refractory(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* refractory_state,
    const nimcp_gpu_tensor_t* spikes,
    float refractory_period,
    float dt
) {
    (void)ctx;

    if (!refractory_state || !spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_axon_refractory: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(refractory_state);
    float* state = tensor_data(refractory_state);
    const float* sp = tensor_data_const(spikes);

    for (uint32_t i = 0; i < n; i++) {
        state[i] = fmaxf(0.0f, state[i] - dt);
        if (sp[i] > 0.5f) {
            state[i] = refractory_period;
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Dendrite Operations (CPU)
//=============================================================================

nimcp_kernel_error_t cpu_dendrite_cable_integrate(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* inputs,
    const nimcp_gpu_tensor_t* cable_Rm,
    const nimcp_gpu_tensor_t* cable_Cm,
    const nimcp_gpu_tensor_t* cable_Ra,
    nimcp_gpu_tensor_t* voltages,
    float dt
) {
    (void)ctx;

    if (!inputs || !cable_Rm || !cable_Cm || !cable_Ra || !voltages) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_dendrite_cable_integrate: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (voltages->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cpu_dendrite_cable_integrate: voltages tensor must have ndim >= 2");
        return NIMCP_KERNEL_ERROR_INVALID_SIZE;
    }

    uint32_t n_dend = voltages->dims[0];
    uint32_t n_seg = voltages->dims[1];

    const float* in = tensor_data_const(inputs);
    const float* Rm = tensor_data_const(cable_Rm);
    const float* Cm = tensor_data_const(cable_Cm);
    const float* Ra = tensor_data_const(cable_Ra);
    float* V = tensor_data(voltages);

    for (uint32_t d = 0; d < n_dend; d++) {
        float tau = Rm[d] * Cm[d];
        float lambda = sqrtf(Rm[d] / Ra[d]);

        for (uint32_t s = 0; s < n_seg; s++) {
            uint32_t idx = d * n_seg + s;
            float V_curr = V[idx];
            float I = in[idx];

            float V_prev = (s > 0) ? V[idx - 1] : V_curr;
            float V_next = (s < n_seg - 1) ? V[idx + 1] : V_curr;

            float d2V = V_prev - 2.0f * V_curr + V_next;
            float dV = (-V_curr + lambda * lambda * d2V + Rm[d] * I) / tau;

            V[idx] = V_curr + dV * dt;
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_dendrite_nmda(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* voltages,
    const nimcp_gpu_tensor_t* mg_block,
    nimcp_gpu_tensor_t* nmda_current,
    nimcp_gpu_tensor_t* nmda_spikes,
    float nmda_threshold
) {
    (void)ctx;

    if (!voltages || !mg_block || !nmda_current || !nmda_spikes) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_dendrite_nmda: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (voltages->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cpu_dendrite_nmda: voltages tensor must have ndim >= 2");
        return NIMCP_KERNEL_ERROR_INVALID_SIZE;
    }

    uint32_t n_dend = voltages->dims[0];
    uint32_t n_seg = voltages->dims[1];

    const float* V = tensor_data_const(voltages);
    const float* mg = tensor_data_const(mg_block);
    float* I_nmda = tensor_data(nmda_current);
    float* spikes = tensor_data(nmda_spikes);

    memset(spikes, 0, n_dend * sizeof(float));

    for (uint32_t d = 0; d < n_dend; d++) {
        for (uint32_t s = 0; s < n_seg; s++) {
            uint32_t idx = d * n_seg + s;
            float v = V[idx];
            float mg_relief = 1.0f / (1.0f + mg[d] * expf(-0.062f * v));
            float I = mg_relief * fmaxf(0.0f, v + 70.0f);
            I_nmda[idx] = I;

            if (I > nmda_threshold) {
                spikes[d] = 1.0f;
            }
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_dendrite_calcium(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* nmda_current,
    const nimcp_gpu_tensor_t* vgcc_current,
    nimcp_gpu_tensor_t* calcium,
    nimcp_gpu_tensor_t* calcium_decay,
    float tau_calcium,
    float dt
) {
    (void)ctx;

    if (!nmda_current || !vgcc_current || !calcium || !calcium_decay) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_dendrite_calcium: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(calcium);
    const float* I_nmda = tensor_data_const(nmda_current);
    const float* I_vgcc = tensor_data_const(vgcc_current);
    float* Ca = tensor_data(calcium);
    float* decay = tensor_data(calcium_decay);

    for (uint32_t i = 0; i < n; i++) {
        float influx = I_nmda[i] * 0.1f + I_vgcc[i] * 0.05f;
        float dCa = influx - Ca[i] / tau_calcium;
        Ca[i] = fmaxf(0.0f, Ca[i] + dCa * dt);
        decay[i] = Ca[i] / tau_calcium;
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_dendrite_bap(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* soma_spike,
    const nimcp_gpu_tensor_t* attenuation,
    nimcp_gpu_tensor_t* bap_signal,
    float bap_velocity,
    float dt
) {
    (void)ctx; (void)bap_velocity;

    if (!soma_spike || !attenuation || !bap_signal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_dendrite_bap: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (bap_signal->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cpu_dendrite_bap: bap_signal tensor must have ndim >= 2");
        return NIMCP_KERNEL_ERROR_INVALID_SIZE;
    }

    uint32_t n_dend = bap_signal->dims[0];
    uint32_t n_seg = bap_signal->dims[1];

    const float* spike = tensor_data_const(soma_spike);
    const float* atten = tensor_data_const(attenuation);
    float* bap = tensor_data(bap_signal);

    for (uint32_t d = 0; d < n_dend; d++) {
        for (uint32_t s = 0; s < n_seg; s++) {
            uint32_t idx = d * n_seg + s;
            float new_bap = spike[d] * atten[idx];
            bap[idx] = fmaxf(new_bap, bap[idx] * expf(-dt / 2.0f));
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Myelin Operations (CPU)
//=============================================================================

nimcp_kernel_error_t cpu_myelin_g_ratio(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* axon_diameter,
    const nimcp_gpu_tensor_t* fiber_diameter,
    nimcp_gpu_tensor_t* g_ratio,
    nimcp_gpu_tensor_t* is_optimal
) {
    (void)ctx;

    if (!axon_diameter || !fiber_diameter || !g_ratio || !is_optimal) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_myelin_g_ratio: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(axon_diameter);
    const float* d_axon = tensor_data_const(axon_diameter);
    const float* d_fiber = tensor_data_const(fiber_diameter);
    float* g = tensor_data(g_ratio);
    float* opt = tensor_data(is_optimal);

    for (uint32_t i = 0; i < n; i++) {
        g[i] = (d_fiber[i] > 0.0f) ? d_axon[i] / d_fiber[i] : 0.0f;
        opt[i] = (fabsf(g[i] - 0.6f) < 0.1f) ? 1.0f : 0.0f;
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_myelin_conduction_velocity(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* g_ratio,
    const nimcp_gpu_tensor_t* internode_length,
    const nimcp_gpu_tensor_t* temperature,
    nimcp_gpu_tensor_t* velocity
) {
    (void)ctx;

    if (!g_ratio || !internode_length || !temperature || !velocity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_myelin_conduction_velocity: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(g_ratio);
    bool scalar_temp = (tensor_size(temperature) == 1);

    const float* g = tensor_data_const(g_ratio);
    const float* L = tensor_data_const(internode_length);
    const float* T = tensor_data_const(temperature);
    float* vel = tensor_data(velocity);

    for (uint32_t i = 0; i < n; i++) {
        float temp = scalar_temp ? T[0] : T[i];
        float temp_factor = powf(1.5f, (temp - 37.0f) / 10.0f);
        float g_factor = 4.0f * g[i] * (1.0f - g[i]);
        float base_velocity = 100.0f * L[i] / 1000.0f;
        vel[i] = base_velocity * g_factor * temp_factor;
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_myelin_plasticity(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity,
    const nimcp_gpu_tensor_t* oligodendrocyte_signal,
    nimcp_gpu_tensor_t* myelin_thickness,
    nimcp_gpu_tensor_t* sheath_length,
    float learning_rate,
    float dt
) {
    (void)ctx;

    if (!activity || !oligodendrocyte_signal ||
        !myelin_thickness || !sheath_length) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_myelin_plasticity: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(activity);
    const float* act = tensor_data_const(activity);
    const float* ol = tensor_data_const(oligodendrocyte_signal);
    float* thick = tensor_data(myelin_thickness);
    float* len = tensor_data(sheath_length);

    for (uint32_t i = 0; i < n; i++) {
        float drive = act[i] * ol[i];
        float d_thick = learning_rate * drive * (2.0f - thick[i]);
        thick[i] = fminf(2.0f, thick[i] + d_thick * dt);

        float d_len = learning_rate * 0.5f * drive * (200.0f - len[i]);
        len[i] = fminf(200.0f, len[i] + d_len * dt);
    }

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Neuromodulator Operations (CPU)
//=============================================================================

nimcp_kernel_error_t cpu_neuromod_decay(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* concentrations,
    const nimcp_gpu_tensor_t* decay_rates,
    float dt
) {
    (void)ctx;

    if (!concentrations || !decay_rates) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_neuromod_decay: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (concentrations->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cpu_neuromod_decay: concentrations tensor must have ndim >= 2");
        return NIMCP_KERNEL_ERROR_INVALID_SIZE;
    }

    uint32_t n_pools = concentrations->dims[0];
    uint32_t n_types = concentrations->dims[1];

    float* conc = tensor_data(concentrations);
    const float* decay = tensor_data_const(decay_rates);

    for (uint32_t p = 0; p < n_pools; p++) {
        for (uint32_t t = 0; t < n_types; t++) {
            uint32_t idx = p * n_types + t;
            conc[idx] *= expf(-dt * decay[t]);
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_neuromod_release(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* release_sites,
    const nimcp_gpu_tensor_t* release_types,
    const nimcp_gpu_tensor_t* release_amounts,
    nimcp_gpu_tensor_t* concentrations,
    uint32_t n_events
) {
    (void)ctx;

    if (!release_sites || !release_types ||
        !release_amounts || !concentrations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_neuromod_release: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n_types = concentrations->dims[1];
    const uint32_t* sites = (const uint32_t*)tensor_data_const(release_sites);
    const uint32_t* types = (const uint32_t*)tensor_data_const(release_types);
    const float* amounts = tensor_data_const(release_amounts);
    float* conc = tensor_data(concentrations);

    for (uint32_t e = 0; e < n_events; e++) {
        uint32_t idx = sites[e] * n_types + types[e];
        conc[idx] += amounts[e];
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_neuromod_effect(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* concentrations,
    const nimcp_gpu_tensor_t* receptor_density,
    nimcp_gpu_tensor_t* modulation
) {
    (void)ctx;

    if (!concentrations || !receptor_density || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_neuromod_effect: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n_synapses = tensor_size(modulation);
    uint32_t n_types = concentrations->dims[1];
    uint32_t n_pools = concentrations->dims[0];

    const float* conc = tensor_data_const(concentrations);
    const float* density = tensor_data_const(receptor_density);
    float* mod = tensor_data(modulation);

    for (uint32_t s = 0; s < n_synapses; s++) {
        float m = 1.0f;
        for (uint32_t t = 0; t < n_types; t++) {
            float dens = density[s * n_types + t];
            float avg_conc = 0.0f;
            for (uint32_t p = 0; p < n_pools; p++) {
                avg_conc += conc[p * n_types + t];
            }
            avg_conc /= (float)n_pools;
            float occupancy = avg_conc / (avg_conc + 0.5f);
            m *= (1.0f + dens * occupancy);
        }
        mod[s] = m;
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_neuromod_phasic_tonic(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phasic_input,
    nimcp_gpu_tensor_t* tonic_level,
    nimcp_gpu_tensor_t* total_level,
    float tonic_tau,
    float phasic_decay,
    float dt
) {
    (void)ctx;

    if (!phasic_input || !tonic_level || !total_level) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_neuromod_phasic_tonic: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    if (phasic_input->ndim < 2) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cpu_neuromod_phasic_tonic: phasic_input tensor must have ndim >= 2");
        return NIMCP_KERNEL_ERROR_INVALID_SIZE;
    }

    uint32_t n_pools = phasic_input->dims[0];
    uint32_t n_types = phasic_input->dims[1];

    const float* phasic = tensor_data_const(phasic_input);
    float* tonic = tensor_data(tonic_level);
    float* total = tensor_data(total_level);

    for (uint32_t p = 0; p < n_pools; p++) {
        for (uint32_t t = 0; t < n_types; t++) {
            uint32_t idx = p * n_types + t;
            tonic[idx] += (0.3f - tonic[idx]) * dt / tonic_tau;
            float phasic_contrib = phasic[idx] * expf(-dt * phasic_decay);
            total[idx] = tonic[idx] + phasic_contrib;
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Glial Operations (CPU)
//=============================================================================

nimcp_kernel_error_t cpu_astrocyte_calcium_wave(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* ip3_levels,
    const nimcp_gpu_tensor_t* gap_junctions,
    nimcp_gpu_tensor_t* calcium,
    nimcp_gpu_tensor_t* wave_front,
    float diffusion_rate,
    float dt
) {
    (void)ctx;

    if (!ip3_levels || !gap_junctions || !calcium || !wave_front) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_astrocyte_calcium_wave: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n_astro = tensor_size(calcium);
    uint32_t n_neighbors = gap_junctions->dims[1];

    const float* ip3 = tensor_data_const(ip3_levels);
    const float* gap = tensor_data_const(gap_junctions);
    float* Ca = tensor_data(calcium);
    float* wave = tensor_data(wave_front);

    // Need temp buffer for updates
    float* Ca_new = (float*)nimcp_malloc(n_astro * sizeof(float));
    if (!Ca_new) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cpu_astrocyte_calcium_wave: failed to allocate temporary buffer");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    for (uint32_t i = 0; i < n_astro; i++) {
        float release = ip3[i] * (1.0f - Ca[i]) * 0.1f;
        float diffusion = 0.0f;

        for (uint32_t n = 0; n < n_neighbors; n++) {
            uint32_t neighbor = (uint32_t)gap[i * n_neighbors + n];
            if (neighbor < n_astro && neighbor != i) {
                float coupling = gap[i * n_neighbors + n];
                diffusion += coupling * (Ca[neighbor] - Ca[i]);
            }
        }

        float decay = Ca[i] * 0.05f;
        float dCa = release + diffusion_rate * diffusion - decay;
        Ca_new[i] = fmaxf(0.0f, fminf(1.0f, Ca[i] + dCa * dt));
        wave[i] = (Ca_new[i] > 0.5f) ? 1.0f : wave[i] * 0.9f;
    }

    memcpy(Ca, Ca_new, n_astro * sizeof(float));
    nimcp_free(Ca_new);

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_astrocyte_release(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* calcium,
    const nimcp_gpu_tensor_t* threshold,
    nimcp_gpu_tensor_t* glutamate_release,
    nimcp_gpu_tensor_t* atp_release
) {
    (void)ctx;

    if (!calcium || !threshold || !glutamate_release || !atp_release) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_astrocyte_release: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(calcium);
    const float* Ca = tensor_data_const(calcium);
    const float* thresh = tensor_data_const(threshold);
    float* glu = tensor_data(glutamate_release);
    float* atp = tensor_data(atp_release);

    for (uint32_t i = 0; i < n; i++) {
        if (Ca[i] > thresh[i]) {
            float excess = Ca[i] - thresh[i];
            glu[i] = excess * 0.5f;
            atp[i] = excess * 0.3f;
        } else {
            glu[i] = 0.0f;
            atp[i] = 0.0f;
        }
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_microglia_activation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* damage_signals,
    const nimcp_gpu_tensor_t* anti_inflam,
    nimcp_gpu_tensor_t* activation_state,
    nimcp_gpu_tensor_t* phagocytic_activity,
    float activation_threshold,
    float dt
) {
    (void)ctx;

    if (!damage_signals || !anti_inflam ||
        !activation_state || !phagocytic_activity) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_microglia_activation: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(damage_signals);
    const float* damage = tensor_data_const(damage_signals);
    const float* anti = tensor_data_const(anti_inflam);
    float* state = tensor_data(activation_state);
    float* phago = tensor_data(phagocytic_activity);

    for (uint32_t i = 0; i < n; i++) {
        float pro_drive = damage[i] - activation_threshold;
        float anti_drive = anti[i] - 0.5f;

        if (pro_drive > 0 && state[i] < 1.0f) {
            state[i] = fminf(1.0f, state[i] + pro_drive * dt);
        } else if (anti_drive > 0 && state[i] > 0.0f) {
            state[i] = fminf(2.0f, fmaxf(0.0f, state[i] + anti_drive * dt));
        } else {
            state[i] *= (1.0f - 0.1f * dt);
        }

        phago[i] = (state[i] > 0.5f && state[i] < 1.5f) ?
                   (1.0f - fabsf(state[i] - 1.0f)) : 0.0f;
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_oligodendrocyte_differentiation(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* activity_signal,
    const nimcp_gpu_tensor_t* growth_factors,
    nimcp_gpu_tensor_t* differentiation_state,
    nimcp_gpu_tensor_t* myelin_production,
    float dt
) {
    (void)ctx;

    if (!activity_signal || !growth_factors ||
        !differentiation_state || !myelin_production) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_oligodendrocyte_differentiation: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(activity_signal);
    const float* activity = tensor_data_const(activity_signal);
    const float* growth = tensor_data_const(growth_factors);
    float* state = tensor_data(differentiation_state);
    float* production = tensor_data(myelin_production);

    for (uint32_t i = 0; i < n; i++) {
        float diff_drive = activity[i] * growth[i];
        float d_state = diff_drive * 0.01f * (1.0f - state[i]);
        state[i] = fminf(1.0f, state[i] + d_state * dt);
        production[i] = state[i] * state[i] * growth[i];
    }

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// Metabolic Operations (CPU)
//=============================================================================

nimcp_kernel_error_t cpu_metabolic_effects(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* atp_levels,
    const nimcp_gpu_tensor_t* oxygen_levels,
    const nimcp_gpu_tensor_t* glucose_levels,
    nimcp_gpu_tensor_t* capacity,
    nimcp_gpu_tensor_t* fatigue
) {
    (void)ctx;

    if (!atp_levels || !oxygen_levels || !glucose_levels ||
        !capacity || !fatigue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_metabolic_effects: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(atp_levels);
    const float* atp = tensor_data_const(atp_levels);
    const float* o2 = tensor_data_const(oxygen_levels);
    const float* glucose = tensor_data_const(glucose_levels);
    float* cap = tensor_data(capacity);
    float* fat = tensor_data(fatigue);

    for (uint32_t i = 0; i < n; i++) {
        cap[i] = fminf(atp[i], fminf(o2[i], glucose[i]));
        fat[i] = (cap[i] < 0.5f) ? (0.5f - cap[i]) * 2.0f : 0.0f;
    }

    return NIMCP_KERNEL_SUCCESS;
}

nimcp_kernel_error_t cpu_metabolic_update(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* neural_activity,
    nimcp_gpu_tensor_t* atp_levels,
    nimcp_gpu_tensor_t* lactate_levels,
    float consumption_rate,
    float recovery_rate,
    float dt
) {
    (void)ctx;

    if (!neural_activity || !atp_levels || !lactate_levels) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cpu_metabolic_update: one or more tensor parameters is NULL");
        return NIMCP_KERNEL_ERROR_NULL_PTR;
    }

    uint32_t n = tensor_size(neural_activity);
    const float* activity = tensor_data_const(neural_activity);
    float* atp = tensor_data(atp_levels);
    float* lactate = tensor_data(lactate_levels);

    for (uint32_t i = 0; i < n; i++) {
        float consumption = activity[i] * consumption_rate * dt;
        atp[i] = fmaxf(0.0f, atp[i] - consumption);

        float recovery = recovery_rate * (1.0f - atp[i]) * dt;
        atp[i] = fminf(1.0f, atp[i] + recovery);

        float lactate_prod = consumption * 0.3f;
        float lactate_clear = lactate[i] * 0.1f * dt;
        lactate[i] = fmaxf(0.0f, lactate[i] + lactate_prod - lactate_clear);
    }

    return NIMCP_KERNEL_SUCCESS;
}

//=============================================================================
// CPU Substrate Ops Table
//=============================================================================

nimcp_substrate_ops_t nimcp_cpu_substrate_ops = {
    // Axon
    .axon_propagate = cpu_axon_propagate,
    .axon_refractory = cpu_axon_refractory,

    // Dendrite
    .dendrite_cable_integrate = cpu_dendrite_cable_integrate,
    .dendrite_nmda = cpu_dendrite_nmda,
    .dendrite_calcium = cpu_dendrite_calcium,
    .dendrite_bap = cpu_dendrite_bap,

    // Myelin
    .myelin_g_ratio = cpu_myelin_g_ratio,
    .myelin_conduction_velocity = cpu_myelin_conduction_velocity,
    .myelin_plasticity = cpu_myelin_plasticity,

    // Neuromodulator
    .neuromod_decay = cpu_neuromod_decay,
    .neuromod_release = cpu_neuromod_release,
    .neuromod_effect = cpu_neuromod_effect,
    .neuromod_phasic_tonic = cpu_neuromod_phasic_tonic,

    // Glial
    .astrocyte_calcium_wave = cpu_astrocyte_calcium_wave,
    .astrocyte_release = cpu_astrocyte_release,
    .microglia_activation = cpu_microglia_activation,
    .oligodendrocyte_differentiation = cpu_oligodendrocyte_differentiation,

    // Metabolic
    .metabolic_effects = cpu_metabolic_effects,
    .metabolic_update = cpu_metabolic_update
};
