//=============================================================================
// nimcp_kernel_backend.h - Unified GPU/CPU Kernel Backend System
//=============================================================================
/**
 * @file nimcp_kernel_backend.h
 * @brief Strategy Pattern for GPU/CPU kernel selection
 *
 * WHAT: Unified interface for CUDA and CPU kernel implementations
 * WHY:  Enables runtime selection of optimal compute backend
 * HOW:  Function pointer tables with automatic backend detection
 *
 * DESIGN PATTERN: Strategy Pattern
 * - Each operation type has a common interface
 * - CUDA and CPU backends implement the same interface
 * - Runtime selection based on hardware availability
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_KERNEL_BACKEND_H
#define NIMCP_KERNEL_BACKEND_H

#include "gpu/context/nimcp_gpu_context.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Backend Types
//=============================================================================

typedef enum {
    NIMCP_BACKEND_CPU = 0,       /**< Pure CPU implementation */
    NIMCP_BACKEND_CUDA = 1,      /**< NVIDIA CUDA implementation */
    NIMCP_BACKEND_ROCM = 2,      /**< AMD ROCm implementation */
    NIMCP_BACKEND_OPENCL = 3,    /**< OpenCL implementation (cross-platform) */
    NIMCP_BACKEND_NEURON = 4,    /**< AWS Inferentia NeuronCore implementation */
    NIMCP_BACKEND_AUTO = 5       /**< Automatic selection: CUDA -> ROCm -> OpenCL -> Neuron -> CPU */
} nimcp_backend_type_t;

typedef enum {
    NIMCP_KERNEL_SUCCESS = 0,
    NIMCP_KERNEL_ERROR_NULL_PTR = -1,
    NIMCP_KERNEL_ERROR_INVALID_SIZE = -2,
    NIMCP_KERNEL_ERROR_DEVICE = -3,
    NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED = -4,
    NIMCP_KERNEL_ERROR_MEMORY = -5
} nimcp_kernel_error_t;

//=============================================================================
// Tensor Operations Interface
//=============================================================================

typedef struct nimcp_tensor_ops {
    // Element-wise operations
    nimcp_kernel_error_t (*add)(nimcp_gpu_context_t* ctx,
                                const nimcp_gpu_tensor_t* a,
                                const nimcp_gpu_tensor_t* b,
                                nimcp_gpu_tensor_t* result);

    nimcp_kernel_error_t (*sub)(nimcp_gpu_context_t* ctx,
                                const nimcp_gpu_tensor_t* a,
                                const nimcp_gpu_tensor_t* b,
                                nimcp_gpu_tensor_t* result);

    nimcp_kernel_error_t (*mul)(nimcp_gpu_context_t* ctx,
                                const nimcp_gpu_tensor_t* a,
                                const nimcp_gpu_tensor_t* b,
                                nimcp_gpu_tensor_t* result);

    nimcp_kernel_error_t (*div)(nimcp_gpu_context_t* ctx,
                                const nimcp_gpu_tensor_t* a,
                                const nimcp_gpu_tensor_t* b,
                                nimcp_gpu_tensor_t* result);

    nimcp_kernel_error_t (*scale)(nimcp_gpu_context_t* ctx,
                                  const nimcp_gpu_tensor_t* a,
                                  float scalar,
                                  nimcp_gpu_tensor_t* result);

    // Matrix operations
    nimcp_kernel_error_t (*matmul)(nimcp_gpu_context_t* ctx,
                                   const nimcp_gpu_tensor_t* a,
                                   const nimcp_gpu_tensor_t* b,
                                   nimcp_gpu_tensor_t* result);

    nimcp_kernel_error_t (*transpose)(nimcp_gpu_context_t* ctx,
                                      const nimcp_gpu_tensor_t* a,
                                      nimcp_gpu_tensor_t* result);

    // Activation functions
    nimcp_kernel_error_t (*relu)(nimcp_gpu_context_t* ctx,
                                 const nimcp_gpu_tensor_t* input,
                                 nimcp_gpu_tensor_t* output);

    nimcp_kernel_error_t (*sigmoid)(nimcp_gpu_context_t* ctx,
                                    const nimcp_gpu_tensor_t* input,
                                    nimcp_gpu_tensor_t* output);

    nimcp_kernel_error_t (*tanh)(nimcp_gpu_context_t* ctx,
                                 const nimcp_gpu_tensor_t* input,
                                 nimcp_gpu_tensor_t* output);

    nimcp_kernel_error_t (*softmax)(nimcp_gpu_context_t* ctx,
                                    const nimcp_gpu_tensor_t* input,
                                    nimcp_gpu_tensor_t* output);

    // Reduction operations
    nimcp_kernel_error_t (*sum)(nimcp_gpu_context_t* ctx,
                                const nimcp_gpu_tensor_t* input,
                                nimcp_gpu_tensor_t* output);

    nimcp_kernel_error_t (*mean)(nimcp_gpu_context_t* ctx,
                                 const nimcp_gpu_tensor_t* input,
                                 nimcp_gpu_tensor_t* output);

    nimcp_kernel_error_t (*max)(nimcp_gpu_context_t* ctx,
                                const nimcp_gpu_tensor_t* input,
                                nimcp_gpu_tensor_t* output);

    nimcp_kernel_error_t (*min)(nimcp_gpu_context_t* ctx,
                                const nimcp_gpu_tensor_t* input,
                                nimcp_gpu_tensor_t* output);
} nimcp_tensor_ops_t;

//=============================================================================
// Training Operations Interface
//=============================================================================

typedef struct nimcp_training_ops {
    // Loss functions
    nimcp_kernel_error_t (*mse_loss)(nimcp_gpu_context_t* ctx,
                                     const nimcp_gpu_tensor_t* pred,
                                     const nimcp_gpu_tensor_t* target,
                                     nimcp_gpu_tensor_t* loss);

    nimcp_kernel_error_t (*cross_entropy_loss)(nimcp_gpu_context_t* ctx,
                                               const nimcp_gpu_tensor_t* pred,
                                               const nimcp_gpu_tensor_t* target,
                                               nimcp_gpu_tensor_t* loss);

    // Gradient operations
    nimcp_kernel_error_t (*gradient_clip)(nimcp_gpu_context_t* ctx,
                                          nimcp_gpu_tensor_t* gradients,
                                          float max_norm);

    nimcp_kernel_error_t (*gradient_accumulate)(nimcp_gpu_context_t* ctx,
                                                nimcp_gpu_tensor_t* accumulated,
                                                const nimcp_gpu_tensor_t* gradients,
                                                float scale);

    // Optimizers
    nimcp_kernel_error_t (*sgd_step)(nimcp_gpu_context_t* ctx,
                                     nimcp_gpu_tensor_t* params,
                                     const nimcp_gpu_tensor_t* gradients,
                                     float learning_rate,
                                     float momentum,
                                     nimcp_gpu_tensor_t* velocity);

    nimcp_kernel_error_t (*adam_step)(nimcp_gpu_context_t* ctx,
                                      nimcp_gpu_tensor_t* params,
                                      const nimcp_gpu_tensor_t* gradients,
                                      nimcp_gpu_tensor_t* m,
                                      nimcp_gpu_tensor_t* v,
                                      float learning_rate,
                                      float beta1, float beta2,
                                      float epsilon, uint64_t t);

    // Backprop
    nimcp_kernel_error_t (*backward_linear)(nimcp_gpu_context_t* ctx,
                                            const nimcp_gpu_tensor_t* grad_output,
                                            const nimcp_gpu_tensor_t* input,
                                            const nimcp_gpu_tensor_t* weights,
                                            nimcp_gpu_tensor_t* grad_input,
                                            nimcp_gpu_tensor_t* grad_weights,
                                            nimcp_gpu_tensor_t* grad_bias);
} nimcp_training_ops_t;

//=============================================================================
// SNN Operations Interface
//=============================================================================

typedef struct nimcp_snn_ops {
    // Neuron models
    nimcp_kernel_error_t (*lif_forward)(nimcp_gpu_context_t* ctx,
                                        const nimcp_gpu_tensor_t* input,
                                        nimcp_gpu_tensor_t* membrane,
                                        nimcp_gpu_tensor_t* spikes,
                                        float tau, float threshold,
                                        float reset, float dt);

    nimcp_kernel_error_t (*izhikevich_forward)(nimcp_gpu_context_t* ctx,
                                               const nimcp_gpu_tensor_t* input,
                                               nimcp_gpu_tensor_t* v,
                                               nimcp_gpu_tensor_t* u,
                                               nimcp_gpu_tensor_t* spikes,
                                               float a, float b, float c, float d,
                                               float dt);

    // Surrogate gradients
    nimcp_kernel_error_t (*surrogate_superspike)(nimcp_gpu_context_t* ctx,
                                                  const nimcp_gpu_tensor_t* input,
                                                  nimcp_gpu_tensor_t* output,
                                                  float beta);

    nimcp_kernel_error_t (*surrogate_fast_sigmoid)(nimcp_gpu_context_t* ctx,
                                                    const nimcp_gpu_tensor_t* input,
                                                    nimcp_gpu_tensor_t* output,
                                                    float slope);

    // STDP
    nimcp_kernel_error_t (*stdp_update)(nimcp_gpu_context_t* ctx,
                                        nimcp_gpu_tensor_t* weights,
                                        const nimcp_gpu_tensor_t* pre_spikes,
                                        const nimcp_gpu_tensor_t* post_spikes,
                                        nimcp_gpu_tensor_t* pre_trace,
                                        nimcp_gpu_tensor_t* post_trace,
                                        float A_plus, float A_minus,
                                        float tau_plus, float tau_minus,
                                        float dt);
} nimcp_snn_ops_t;

//=============================================================================
// CNN Operations Interface
//=============================================================================

typedef struct nimcp_cnn_ops {
    // Convolution
    nimcp_kernel_error_t (*conv2d_forward)(nimcp_gpu_context_t* ctx,
                                           const nimcp_gpu_tensor_t* input,
                                           const nimcp_gpu_tensor_t* kernel,
                                           const nimcp_gpu_tensor_t* bias,
                                           nimcp_gpu_tensor_t* output,
                                           uint32_t stride, uint32_t padding);

    nimcp_kernel_error_t (*conv2d_backward)(nimcp_gpu_context_t* ctx,
                                            const nimcp_gpu_tensor_t* grad_output,
                                            const nimcp_gpu_tensor_t* input,
                                            const nimcp_gpu_tensor_t* kernel,
                                            nimcp_gpu_tensor_t* grad_input,
                                            nimcp_gpu_tensor_t* grad_kernel,
                                            nimcp_gpu_tensor_t* grad_bias,
                                            uint32_t stride, uint32_t padding);

    // Pooling
    nimcp_kernel_error_t (*maxpool2d)(nimcp_gpu_context_t* ctx,
                                      const nimcp_gpu_tensor_t* input,
                                      nimcp_gpu_tensor_t* output,
                                      nimcp_gpu_tensor_t* indices,
                                      uint32_t kernel_size, uint32_t stride);

    nimcp_kernel_error_t (*avgpool2d)(nimcp_gpu_context_t* ctx,
                                      const nimcp_gpu_tensor_t* input,
                                      nimcp_gpu_tensor_t* output,
                                      uint32_t kernel_size, uint32_t stride);

    // Normalization
    nimcp_kernel_error_t (*batchnorm_forward)(nimcp_gpu_context_t* ctx,
                                              const nimcp_gpu_tensor_t* input,
                                              const nimcp_gpu_tensor_t* gamma,
                                              const nimcp_gpu_tensor_t* beta,
                                              nimcp_gpu_tensor_t* output,
                                              nimcp_gpu_tensor_t* mean,
                                              nimcp_gpu_tensor_t* var,
                                              float epsilon, bool training);
} nimcp_cnn_ops_t;

//=============================================================================
// LNN Operations Interface
//=============================================================================

// Forward declare LNN types
struct nimcp_lnn_layer_gpu;
struct nimcp_lnn_ode_config;

typedef struct nimcp_lnn_ops {
    // ODE solvers
    nimcp_kernel_error_t (*euler_step)(nimcp_gpu_context_t* ctx,
                                       const nimcp_gpu_tensor_t* x,
                                       const nimcp_gpu_tensor_t* dx_dt,
                                       float dt,
                                       nimcp_gpu_tensor_t* x_new);

    nimcp_kernel_error_t (*heun_step)(nimcp_gpu_context_t* ctx,
                                      struct nimcp_lnn_layer_gpu* layer,
                                      const nimcp_gpu_tensor_t* input,
                                      float dt,
                                      const struct nimcp_lnn_ode_config* config);

    nimcp_kernel_error_t (*rk4_step)(nimcp_gpu_context_t* ctx,
                                     struct nimcp_lnn_layer_gpu* layer,
                                     const nimcp_gpu_tensor_t* input,
                                     float dt,
                                     const struct nimcp_lnn_ode_config* config);

    nimcp_kernel_error_t (*dopri5_step)(nimcp_gpu_context_t* ctx,
                                        struct nimcp_lnn_layer_gpu* layer,
                                        const nimcp_gpu_tensor_t* input,
                                        float* dt_ptr,
                                        const struct nimcp_lnn_ode_config* config);

    // LTC dynamics
    nimcp_kernel_error_t (*compute_derivative)(nimcp_gpu_context_t* ctx,
                                               struct nimcp_lnn_layer_gpu* layer,
                                               const nimcp_gpu_tensor_t* input,
                                               nimcp_gpu_tensor_t* dx_dt);

    nimcp_kernel_error_t (*update_tau)(nimcp_gpu_context_t* ctx,
                                       struct nimcp_lnn_layer_gpu* layer,
                                       const nimcp_gpu_tensor_t* input);

    // Sparse operations
    nimcp_kernel_error_t (*sparse_matvec)(nimcp_gpu_context_t* ctx,
                                          const nimcp_gpu_tensor_t* row_ptr,
                                          const nimcp_gpu_tensor_t* col_idx,
                                          const nimcp_gpu_tensor_t* values,
                                          const nimcp_gpu_tensor_t* x,
                                          nimcp_gpu_tensor_t* y,
                                          uint32_t n_rows, float alpha);
} nimcp_lnn_ops_t;

//=============================================================================
// Quantum Operations Interface
//=============================================================================

// Forward declare quantum types
struct nimcp_quantum_state;
struct nimcp_ising_model;
struct nimcp_grover_config;
struct nimcp_annealing_config;

typedef struct nimcp_quantum_ops {
    // Quantum state
    struct nimcp_quantum_state* (*state_create)(nimcp_gpu_context_t* ctx,
                                                 uint32_t n_qubits);

    void (*state_destroy)(struct nimcp_quantum_state* state);

    nimcp_kernel_error_t (*state_hadamard_all)(nimcp_gpu_context_t* ctx,
                                               struct nimcp_quantum_state* state);

    nimcp_kernel_error_t (*apply_gate)(nimcp_gpu_context_t* ctx,
                                       struct nimcp_quantum_state* state,
                                       uint32_t qubit_idx,
                                       const float gate_real[2][2],
                                       const float gate_imag[2][2]);

    nimcp_kernel_error_t (*measure)(nimcp_gpu_context_t* ctx,
                                    struct nimcp_quantum_state* state,
                                    uint32_t* measured_state,
                                    float* probability);

    // Grover's algorithm
    nimcp_kernel_error_t (*grover_oracle)(nimcp_gpu_context_t* ctx,
                                          struct nimcp_quantum_state* state,
                                          const uint32_t* marked_states,
                                          uint32_t n_marked);

    nimcp_kernel_error_t (*grover_diffusion)(nimcp_gpu_context_t* ctx,
                                             struct nimcp_quantum_state* state);

    nimcp_kernel_error_t (*grover_search)(nimcp_gpu_context_t* ctx,
                                          const struct nimcp_grover_config* config,
                                          uint32_t* found_state,
                                          bool* success);

    // Quantum annealing
    struct nimcp_ising_model* (*ising_create)(nimcp_gpu_context_t* ctx,
                                               uint32_t n_spins);

    void (*ising_destroy)(struct nimcp_ising_model* model);

    float (*quantum_anneal)(nimcp_gpu_context_t* ctx,
                            struct nimcp_ising_model* model,
                            const struct nimcp_annealing_config* config);
} nimcp_quantum_ops_t;

//=============================================================================
// Inference Operations Interface
//=============================================================================

typedef struct nimcp_inference_ops {
    // Fused operations
    nimcp_kernel_error_t (*linear_relu)(nimcp_gpu_context_t* ctx,
                                        const nimcp_gpu_tensor_t* input,
                                        const nimcp_gpu_tensor_t* weights,
                                        const nimcp_gpu_tensor_t* bias,
                                        nimcp_gpu_tensor_t* output);

    nimcp_kernel_error_t (*conv_bn_relu)(nimcp_gpu_context_t* ctx,
                                         const nimcp_gpu_tensor_t* input,
                                         const nimcp_gpu_tensor_t* kernel,
                                         const nimcp_gpu_tensor_t* bn_params,
                                         nimcp_gpu_tensor_t* output,
                                         uint32_t stride, uint32_t padding);

    // Quantized operations
    nimcp_kernel_error_t (*quantize_int8)(nimcp_gpu_context_t* ctx,
                                          const nimcp_gpu_tensor_t* input,
                                          nimcp_gpu_tensor_t* output,
                                          float scale, int8_t zero_point);

    nimcp_kernel_error_t (*dequantize_int8)(nimcp_gpu_context_t* ctx,
                                            const nimcp_gpu_tensor_t* input,
                                            nimcp_gpu_tensor_t* output,
                                            float scale, int8_t zero_point);

    nimcp_kernel_error_t (*matmul_int8)(nimcp_gpu_context_t* ctx,
                                        const nimcp_gpu_tensor_t* a,
                                        const nimcp_gpu_tensor_t* b,
                                        nimcp_gpu_tensor_t* result,
                                        float scale_a, float scale_b,
                                        float scale_out);
} nimcp_inference_ops_t;

//=============================================================================
// Neural Substrate Operations Interface
//=============================================================================
/**
 * @brief Unified interface for neural substrate GPU/CPU operations
 *
 * WHAT: Common interface for axon, dendrite, myelin, neuromodulator, and glial ops
 * WHY:  Enables runtime backend selection without separate GPU modules
 * HOW:  Strategy pattern with tensor-based parameters for all operations
 *
 * DESIGN: All parameters are tensors to enable:
 * - Batch processing across populations
 * - Automatic GPU acceleration
 * - Gradient computation for learning
 */
typedef struct nimcp_substrate_ops {
    //=========================================================================
    // Axon Operations
    //=========================================================================

    /** @brief Propagate signals along axons with myelination effects */
    nimcp_kernel_error_t (*axon_propagate)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* input_signals,    /**< [N_axons] input spikes */
        const nimcp_gpu_tensor_t* velocities,       /**< [N_axons] conduction velocity */
        const nimcp_gpu_tensor_t* myelination,      /**< [N_axons] myelin factor 0-1 */
        const nimcp_gpu_tensor_t* lengths,          /**< [N_axons] axon lengths */
        nimcp_gpu_tensor_t* output_signals,         /**< [N_axons] propagated signals */
        nimcp_gpu_tensor_t* delays,                 /**< [N_axons] computed delays */
        float dt);

    /** @brief Update axon refractory states */
    nimcp_kernel_error_t (*axon_refractory)(
        nimcp_gpu_context_t* ctx,
        nimcp_gpu_tensor_t* refractory_state,       /**< [N_axons] refractory timers */
        const nimcp_gpu_tensor_t* spikes,           /**< [N_axons] new spikes */
        float refractory_period,
        float dt);

    //=========================================================================
    // Dendrite Operations
    //=========================================================================

    /** @brief Cable equation integration for dendritic segments */
    nimcp_kernel_error_t (*dendrite_cable_integrate)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* inputs,           /**< [N_dend, N_seg] synaptic inputs */
        const nimcp_gpu_tensor_t* cable_Rm,         /**< [N_dend] membrane resistance */
        const nimcp_gpu_tensor_t* cable_Cm,         /**< [N_dend] membrane capacitance */
        const nimcp_gpu_tensor_t* cable_Ra,         /**< [N_dend] axial resistance */
        nimcp_gpu_tensor_t* voltages,               /**< [N_dend, N_seg] segment voltages */
        float dt);

    /** @brief NMDA spike detection and amplification */
    nimcp_kernel_error_t (*dendrite_nmda)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* voltages,         /**< [N_dend, N_seg] segment voltages */
        const nimcp_gpu_tensor_t* mg_block,         /**< [N_dend] Mg2+ block factor */
        nimcp_gpu_tensor_t* nmda_current,           /**< [N_dend, N_seg] NMDA currents */
        nimcp_gpu_tensor_t* nmda_spikes,            /**< [N_dend] detected NMDA spikes */
        float nmda_threshold);

    /** @brief Calcium dynamics in dendritic spines */
    nimcp_kernel_error_t (*dendrite_calcium)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* nmda_current,     /**< [N_spines] NMDA calcium influx */
        const nimcp_gpu_tensor_t* vgcc_current,     /**< [N_spines] VGCC calcium influx */
        nimcp_gpu_tensor_t* calcium,                /**< [N_spines] calcium concentration */
        nimcp_gpu_tensor_t* calcium_decay,          /**< [N_spines] decay state */
        float tau_calcium,
        float dt);

    /** @brief Backpropagating action potential */
    nimcp_kernel_error_t (*dendrite_bap)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* soma_spike,       /**< [N_neurons] somatic spikes */
        const nimcp_gpu_tensor_t* attenuation,      /**< [N_dend, N_seg] distance attenuation */
        nimcp_gpu_tensor_t* bap_signal,             /**< [N_dend, N_seg] bAP amplitude */
        float bap_velocity,
        float dt);

    //=========================================================================
    // Myelin Sheath Operations
    //=========================================================================

    /** @brief Compute optimal G-ratio (Rushton's formula) */
    nimcp_kernel_error_t (*myelin_g_ratio)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* axon_diameter,    /**< [N_axons] axon diameters */
        const nimcp_gpu_tensor_t* fiber_diameter,   /**< [N_axons] total fiber diameters */
        nimcp_gpu_tensor_t* g_ratio,                /**< [N_axons] computed G-ratios */
        nimcp_gpu_tensor_t* is_optimal);            /**< [N_axons] optimality flags */

    /** @brief Saltatory conduction velocity calculation */
    nimcp_kernel_error_t (*myelin_conduction_velocity)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* g_ratio,          /**< [N_axons] G-ratios */
        const nimcp_gpu_tensor_t* internode_length, /**< [N_axons] internode lengths */
        const nimcp_gpu_tensor_t* temperature,      /**< [1] or [N_axons] temperature */
        nimcp_gpu_tensor_t* velocity);              /**< [N_axons] conduction velocities */

    /** @brief Activity-dependent myelination plasticity */
    nimcp_kernel_error_t (*myelin_plasticity)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* activity,         /**< [N_axons] neural activity */
        const nimcp_gpu_tensor_t* oligodendrocyte_signal, /**< [N_axons] OL signals */
        nimcp_gpu_tensor_t* myelin_thickness,       /**< [N_axons] myelin thickness */
        nimcp_gpu_tensor_t* sheath_length,          /**< [N_axons] sheath lengths */
        float learning_rate,
        float dt);

    //=========================================================================
    // Neuromodulator Operations
    //=========================================================================

    /** @brief Update neuromodulator concentrations with decay */
    nimcp_kernel_error_t (*neuromod_decay)(
        nimcp_gpu_context_t* ctx,
        nimcp_gpu_tensor_t* concentrations,         /**< [N_pools, N_types] DA,5HT,ACh,NE */
        const nimcp_gpu_tensor_t* decay_rates,      /**< [N_types] per-type decay rates */
        float dt);

    /** @brief Process neuromodulator release events */
    nimcp_kernel_error_t (*neuromod_release)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* release_sites,    /**< [N_events] release site IDs */
        const nimcp_gpu_tensor_t* release_types,    /**< [N_events] neuromod types */
        const nimcp_gpu_tensor_t* release_amounts,  /**< [N_events] release amounts */
        nimcp_gpu_tensor_t* concentrations,         /**< [N_pools, N_types] updated */
        uint32_t n_events);

    /** @brief Compute synaptic modulation from neuromodulator levels */
    nimcp_kernel_error_t (*neuromod_effect)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* concentrations,   /**< [N_pools, N_types] levels */
        const nimcp_gpu_tensor_t* receptor_density, /**< [N_synapses, N_types] densities */
        nimcp_gpu_tensor_t* modulation);            /**< [N_synapses] modulation factor */

    /** @brief Phasic-tonic neuromodulator dynamics */
    nimcp_kernel_error_t (*neuromod_phasic_tonic)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* phasic_input,     /**< [N_pools, N_types] phasic bursts */
        nimcp_gpu_tensor_t* tonic_level,            /**< [N_pools, N_types] tonic baseline */
        nimcp_gpu_tensor_t* total_level,            /**< [N_pools, N_types] combined */
        float tonic_tau,
        float phasic_decay,
        float dt);

    //=========================================================================
    // Glial Cell Operations
    //=========================================================================

    /** @brief Astrocyte calcium wave propagation */
    nimcp_kernel_error_t (*astrocyte_calcium_wave)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* ip3_levels,       /**< [N_astro] IP3 concentrations */
        const nimcp_gpu_tensor_t* gap_junctions,    /**< [N_astro, N_neighbors] connectivity */
        nimcp_gpu_tensor_t* calcium,                /**< [N_astro] calcium levels */
        nimcp_gpu_tensor_t* wave_front,             /**< [N_astro] wave propagation state */
        float diffusion_rate,
        float dt);

    /** @brief Astrocyte gliotransmitter release */
    nimcp_kernel_error_t (*astrocyte_release)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* calcium,          /**< [N_astro] calcium levels */
        const nimcp_gpu_tensor_t* threshold,        /**< [N_astro] release thresholds */
        nimcp_gpu_tensor_t* glutamate_release,      /**< [N_astro] glutamate released */
        nimcp_gpu_tensor_t* atp_release);           /**< [N_astro] ATP released */

    /** @brief Microglia activation state update */
    nimcp_kernel_error_t (*microglia_activation)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* damage_signals,   /**< [N_micro] ATP/cytokine levels */
        const nimcp_gpu_tensor_t* anti_inflam,      /**< [N_micro] anti-inflammatory */
        nimcp_gpu_tensor_t* activation_state,       /**< [N_micro] M0/M1/M2 state */
        nimcp_gpu_tensor_t* phagocytic_activity,    /**< [N_micro] phagocytosis rate */
        float activation_threshold,
        float dt);

    /** @brief Oligodendrocyte precursor differentiation */
    nimcp_kernel_error_t (*oligodendrocyte_differentiation)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* activity_signal,  /**< [N_opc] neural activity */
        const nimcp_gpu_tensor_t* growth_factors,   /**< [N_opc] PDGF, FGF levels */
        nimcp_gpu_tensor_t* differentiation_state,  /**< [N_opc] OPC->OL progress */
        nimcp_gpu_tensor_t* myelin_production,      /**< [N_opc] myelin output rate */
        float dt);

    //=========================================================================
    // Metabolic Operations
    //=========================================================================

    /** @brief Compute metabolic effects on neural activity */
    nimcp_kernel_error_t (*metabolic_effects)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* atp_levels,       /**< [N_regions] ATP availability */
        const nimcp_gpu_tensor_t* oxygen_levels,    /**< [N_regions] oxygen levels */
        const nimcp_gpu_tensor_t* glucose_levels,   /**< [N_regions] glucose levels */
        nimcp_gpu_tensor_t* capacity,               /**< [N_regions] metabolic capacity */
        nimcp_gpu_tensor_t* fatigue);               /**< [N_regions] fatigue factor */

    /** @brief Update metabolic state based on activity */
    nimcp_kernel_error_t (*metabolic_update)(
        nimcp_gpu_context_t* ctx,
        const nimcp_gpu_tensor_t* neural_activity,  /**< [N_regions] activity levels */
        nimcp_gpu_tensor_t* atp_levels,             /**< [N_regions] ATP (consumed) */
        nimcp_gpu_tensor_t* lactate_levels,         /**< [N_regions] lactate (produced) */
        float consumption_rate,
        float recovery_rate,
        float dt);

} nimcp_substrate_ops_t;

//=============================================================================
// Unified Kernel Backend
//=============================================================================

/**
 * @brief Complete kernel backend with all operation interfaces
 */
typedef struct nimcp_kernel_backend {
    nimcp_backend_type_t type;        /**< Current backend type */
    const char* name;                  /**< Backend name string */
    bool initialized;                  /**< Initialization status */

    // Operation tables
    nimcp_tensor_ops_t tensor;         /**< Tensor operations */
    nimcp_training_ops_t training;     /**< Training operations */
    nimcp_snn_ops_t snn;               /**< SNN operations */
    nimcp_cnn_ops_t cnn;               /**< CNN operations */
    nimcp_lnn_ops_t lnn;               /**< LNN operations */
    nimcp_quantum_ops_t quantum;       /**< Quantum operations */
    nimcp_inference_ops_t inference;   /**< Inference operations */
    nimcp_substrate_ops_t substrate;   /**< Neural substrate operations */
} nimcp_kernel_backend_t;

//=============================================================================
// Backend API
//=============================================================================

/**
 * @brief Initialize the kernel backend system
 *
 * @param preferred Preferred backend type (use AUTO for automatic selection)
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_kernel_backend_init(nimcp_backend_type_t preferred);

/**
 * @brief Initialize kernel backend with GPU-first default policy
 *
 * WHAT: Tries GPU backends in order (CUDA -> ROCm -> OpenCL) before CPU
 * WHY:  Phase 1 GPU integration - GPU is now the default backend
 * HOW:  Auto-detects available GPU backends and selects best available
 *
 * FALLBACK PRIORITY:
 * 1. CUDA (NVIDIA GPUs)
 * 2. ROCm (AMD GPUs)
 * 3. OpenCL (Cross-platform GPU)
 * 4. CPU (Always available fallback)
 *
 * @return true on success (always succeeds - CPU fallback guaranteed)
 */
NIMCP_EXPORT bool nimcp_kernel_backend_init_default(void);

/**
 * @brief Shutdown the kernel backend system
 */
NIMCP_EXPORT void nimcp_kernel_backend_shutdown(void);

/**
 * @brief Get the current kernel backend
 *
 * @return Pointer to the active backend (never NULL after init)
 */
NIMCP_EXPORT nimcp_kernel_backend_t* nimcp_get_kernel_backend(void);

/**
 * @brief Check if CUDA backend is available
 *
 * @return true if CUDA is available and initialized
 */
NIMCP_EXPORT bool nimcp_cuda_backend_available(void);

/**
 * @brief Get the current backend type
 *
 * @return Current backend type
 */
NIMCP_EXPORT nimcp_backend_type_t nimcp_get_backend_type(void);

/**
 * @brief Switch backend at runtime (if available)
 *
 * @param type Desired backend type
 * @return true if switch succeeded
 */
NIMCP_EXPORT bool nimcp_switch_backend(nimcp_backend_type_t type);

/**
 * @brief Get backend name string
 *
 * @param type Backend type
 * @return Name string
 */
NIMCP_EXPORT const char* nimcp_backend_type_name(nimcp_backend_type_t type);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Get tensor operations from current backend
 */
#define NIMCP_TENSOR_OPS() (&nimcp_get_kernel_backend()->tensor)

/**
 * @brief Get training operations from current backend
 */
#define NIMCP_TRAINING_OPS() (&nimcp_get_kernel_backend()->training)

/**
 * @brief Get SNN operations from current backend
 */
#define NIMCP_SNN_OPS() (&nimcp_get_kernel_backend()->snn)

/**
 * @brief Get CNN operations from current backend
 */
#define NIMCP_CNN_OPS() (&nimcp_get_kernel_backend()->cnn)

/**
 * @brief Get LNN operations from current backend
 */
#define NIMCP_LNN_OPS() (&nimcp_get_kernel_backend()->lnn)

/**
 * @brief Get quantum operations from current backend
 */
#define NIMCP_QUANTUM_OPS() (&nimcp_get_kernel_backend()->quantum)

/**
 * @brief Get inference operations from current backend
 */
#define NIMCP_INFERENCE_OPS() (&nimcp_get_kernel_backend()->inference)

/**
 * @brief Call tensor operation with error handling
 */
#define NIMCP_CALL_TENSOR_OP(op, ...) \
    (NIMCP_TENSOR_OPS()->op ? NIMCP_TENSOR_OPS()->op(__VA_ARGS__) : NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED)

#ifdef __cplusplus
}
#endif

#endif // NIMCP_KERNEL_BACKEND_H
