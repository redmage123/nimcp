/**
 * @file nimcp_substrate_gpu.h
 * @brief Unified GPU Neural Substrate Integration API
 *
 * WHAT: High-level API for GPU-accelerated neural substrate components
 * WHY:  Provides cohesive interface for axon, dendrite, myelin, glial, and neuromodulator ops
 * HOW:  Wraps kernel backend substrate ops with context management and convenience functions
 *
 * BIOLOGICAL BASIS:
 * =================
 * The neural substrate encompasses all cellular and molecular components that support
 * neural computation beyond simple neuron firing:
 *
 * - AXONS: Signal propagation with myelination-dependent velocity
 * - DENDRITES: Cable equation integration, NMDA spikes, calcium dynamics
 * - MYELIN: Activity-dependent plasticity, G-ratio optimization
 * - NEUROMODULATORS: Dopamine, serotonin, acetylcholine, norepinephrine dynamics
 * - GLIAL CELLS: Astrocyte calcium waves, microglia activation, OPC differentiation
 * - METABOLIC: ATP/glucose/oxygen constraints on neural activity
 *
 * GPU ACCELERATION RATIONALE:
 * ===========================
 * Large-scale neural simulations require processing millions of:
 * - Axon segments (signal propagation, refractory dynamics)
 * - Dendritic compartments (cable equation, NMDA, calcium)
 * - Myelin sheaths (plasticity, conduction velocity)
 * - Neuromodulator pools (release, decay, receptor binding)
 * - Glial cells (calcium waves, gliotransmission)
 *
 * USAGE:
 * ======
 * @code
 * // Initialize substrate context
 * substrate_gpu_context_t* ctx = substrate_gpu_create(gpu_ctx, &config);
 *
 * // Process axon signals
 * substrate_gpu_axon_propagate(ctx, input_signals, dt);
 *
 * // Update dendrite voltages
 * substrate_gpu_dendrite_step(ctx, synaptic_inputs, dt);
 *
 * // Update neuromodulator levels
 * substrate_gpu_neuromod_step(ctx, release_events, dt);
 *
 * substrate_gpu_destroy(ctx);
 * @endcode
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_SUBSTRATE_GPU_H
#define NIMCP_SUBSTRATE_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "gpu/backend/nimcp_kernel_backend.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct substrate_gpu_context substrate_gpu_context_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Axon subsystem configuration
 */
typedef struct {
    uint32_t max_axons;              /**< Maximum number of axons */
    uint32_t max_segments;           /**< Maximum segments per axon */
    float refractory_period_ms;      /**< Default refractory period (ms) */
    float base_velocity_ms;          /**< Base conduction velocity (m/s) */
    float myelin_multiplier;         /**< Velocity boost for myelinated axons */
} substrate_axon_config_t;

/**
 * @brief Dendrite subsystem configuration
 */
typedef struct {
    uint32_t max_dendrites;          /**< Maximum number of dendrites */
    uint32_t max_segments;           /**< Maximum segments per dendrite */
    uint32_t max_spines;             /**< Maximum dendritic spines */
    float default_Rm;                /**< Default membrane resistance (ohm-cm2) */
    float default_Cm;                /**< Default membrane capacitance (uF/cm2) */
    float default_Ra;                /**< Default axial resistance (ohm-cm) */
    float nmda_threshold;            /**< NMDA spike threshold */
    float tau_calcium_ms;            /**< Calcium decay time constant (ms) */
} substrate_dendrite_config_t;

/**
 * @brief Myelin subsystem configuration
 */
typedef struct {
    uint32_t max_sheaths;            /**< Maximum myelin sheaths */
    float optimal_g_ratio;           /**< Target G-ratio (typically 0.6) */
    float plasticity_rate;           /**< Myelin plasticity learning rate */
    float max_thickness_um;          /**< Maximum myelin thickness (um) */
    float max_internode_um;          /**< Maximum internode length (um) */
    float temperature_c;             /**< Temperature for Q10 calculations */
} substrate_myelin_config_t;

/**
 * @brief Neuromodulator subsystem configuration
 */
typedef struct {
    uint32_t max_pools;              /**< Maximum neuromodulator pools */
    uint32_t n_types;                /**< Number of neuromodulator types (default 4: DA,5HT,ACh,NE) */
    float tonic_tau_ms;              /**< Tonic level time constant (ms) */
    float phasic_decay;              /**< Phasic burst decay rate */
    float* decay_rates;              /**< Per-type decay rates (NULL for defaults) */
} substrate_neuromod_config_t;

/**
 * @brief Glial subsystem configuration
 */
typedef struct {
    uint32_t max_astrocytes;         /**< Maximum astrocytes */
    uint32_t max_microglia;          /**< Maximum microglia */
    uint32_t max_opcs;               /**< Maximum oligodendrocyte precursor cells */
    uint32_t max_neighbors;          /**< Maximum gap junction neighbors */
    float calcium_diffusion_rate;    /**< Astrocyte calcium diffusion rate */
    float microglia_threshold;       /**< Microglia activation threshold */
} substrate_glial_config_t;

/**
 * @brief Metabolic subsystem configuration
 */
typedef struct {
    uint32_t n_regions;              /**< Number of metabolic regions */
    float atp_consumption_rate;      /**< ATP consumption per activity unit */
    float atp_recovery_rate;         /**< ATP recovery rate */
    float lactate_clearance_rate;    /**< Lactate clearance rate */
} substrate_metabolic_config_t;

/**
 * @brief Complete substrate GPU configuration
 */
typedef struct {
    substrate_axon_config_t axon;
    substrate_dendrite_config_t dendrite;
    substrate_myelin_config_t myelin;
    substrate_neuromod_config_t neuromod;
    substrate_glial_config_t glial;
    substrate_metabolic_config_t metabolic;
    bool enable_async_ops;           /**< Enable async GPU operations */
    bool enable_mixed_precision;     /**< Enable FP16 where appropriate */
} substrate_gpu_config_t;

/**
 * @brief Get default substrate GPU configuration
 */
NIMCP_EXPORT substrate_gpu_config_t substrate_gpu_default_config(void);

//=============================================================================
// Tensor Storage
//=============================================================================

/**
 * @brief Axon tensor storage
 */
typedef struct {
    nimcp_gpu_tensor_t* signals;         /**< [N_axons] current signal values */
    nimcp_gpu_tensor_t* velocities;      /**< [N_axons] conduction velocities */
    nimcp_gpu_tensor_t* myelination;     /**< [N_axons] myelination levels 0-1 */
    nimcp_gpu_tensor_t* lengths;         /**< [N_axons] axon lengths */
    nimcp_gpu_tensor_t* delays;          /**< [N_axons] propagation delays */
    nimcp_gpu_tensor_t* refractory;      /**< [N_axons] refractory state timers */
    uint32_t n_axons;
} substrate_axon_tensors_t;

/**
 * @brief Dendrite tensor storage
 */
typedef struct {
    nimcp_gpu_tensor_t* voltages;        /**< [N_dend, N_seg] segment voltages */
    nimcp_gpu_tensor_t* cable_Rm;        /**< [N_dend] membrane resistance */
    nimcp_gpu_tensor_t* cable_Cm;        /**< [N_dend] membrane capacitance */
    nimcp_gpu_tensor_t* cable_Ra;        /**< [N_dend] axial resistance */
    nimcp_gpu_tensor_t* mg_block;        /**< [N_dend] Mg2+ block factors */
    nimcp_gpu_tensor_t* nmda_current;    /**< [N_dend, N_seg] NMDA currents */
    nimcp_gpu_tensor_t* nmda_spikes;     /**< [N_dend] detected NMDA spikes */
    nimcp_gpu_tensor_t* calcium;         /**< [N_spines] spine calcium levels */
    nimcp_gpu_tensor_t* calcium_decay;   /**< [N_spines] calcium decay state */
    nimcp_gpu_tensor_t* vgcc_current;    /**< [N_spines] VGCC calcium influx */
    nimcp_gpu_tensor_t* bap_signal;      /**< [N_dend, N_seg] bAP amplitudes */
    nimcp_gpu_tensor_t* bap_attenuation; /**< [N_dend, N_seg] distance attenuation */
    nimcp_gpu_tensor_t* soma_spike;      /**< [N_neurons] somatic spikes */
    uint32_t n_dendrites;
    uint32_t n_segments;
    uint32_t n_spines;
} substrate_dendrite_tensors_t;

/**
 * @brief Myelin tensor storage
 */
typedef struct {
    nimcp_gpu_tensor_t* axon_diameter;   /**< [N_axons] axon diameters */
    nimcp_gpu_tensor_t* fiber_diameter;  /**< [N_axons] total fiber diameters */
    nimcp_gpu_tensor_t* g_ratio;         /**< [N_axons] computed G-ratios */
    nimcp_gpu_tensor_t* is_optimal;      /**< [N_axons] optimality flags */
    nimcp_gpu_tensor_t* internode_length;/**< [N_axons] internode lengths */
    nimcp_gpu_tensor_t* temperature;     /**< [1] or [N_axons] temperature */
    nimcp_gpu_tensor_t* velocity;        /**< [N_axons] conduction velocities */
    nimcp_gpu_tensor_t* activity;        /**< [N_axons] neural activity levels */
    nimcp_gpu_tensor_t* ol_signal;       /**< [N_axons] oligodendrocyte signals */
    nimcp_gpu_tensor_t* thickness;       /**< [N_axons] myelin thickness */
    nimcp_gpu_tensor_t* sheath_length;   /**< [N_axons] sheath lengths */
    uint32_t n_axons;
} substrate_myelin_tensors_t;

/**
 * @brief Neuromodulator tensor storage
 */
typedef struct {
    nimcp_gpu_tensor_t* concentrations;  /**< [N_pools, N_types] neuromod levels */
    nimcp_gpu_tensor_t* decay_rates;     /**< [N_types] per-type decay rates */
    nimcp_gpu_tensor_t* tonic_level;     /**< [N_pools, N_types] tonic baseline */
    nimcp_gpu_tensor_t* total_level;     /**< [N_pools, N_types] combined levels */
    nimcp_gpu_tensor_t* receptor_density;/**< [N_synapses, N_types] receptor densities */
    nimcp_gpu_tensor_t* modulation;      /**< [N_synapses] modulation factors */
    uint32_t n_pools;
    uint32_t n_types;
    uint32_t n_synapses;
} substrate_neuromod_tensors_t;

/**
 * @brief Glial tensor storage
 */
typedef struct {
    // Astrocyte tensors
    nimcp_gpu_tensor_t* astro_calcium;   /**< [N_astro] calcium levels */
    nimcp_gpu_tensor_t* astro_ip3;       /**< [N_astro] IP3 levels */
    nimcp_gpu_tensor_t* astro_gaps;      /**< [N_astro, N_neighbors] gap junctions */
    nimcp_gpu_tensor_t* astro_wave;      /**< [N_astro] wave propagation state */
    nimcp_gpu_tensor_t* astro_threshold; /**< [N_astro] release thresholds */
    nimcp_gpu_tensor_t* astro_glu;       /**< [N_astro] glutamate release */
    nimcp_gpu_tensor_t* astro_atp;       /**< [N_astro] ATP release */

    // Microglia tensors
    nimcp_gpu_tensor_t* micro_damage;    /**< [N_micro] damage signals */
    nimcp_gpu_tensor_t* micro_anti;      /**< [N_micro] anti-inflammatory */
    nimcp_gpu_tensor_t* micro_state;     /**< [N_micro] M0/M1/M2 state */
    nimcp_gpu_tensor_t* micro_phago;     /**< [N_micro] phagocytic activity */

    // OPC tensors
    nimcp_gpu_tensor_t* opc_activity;    /**< [N_opc] activity signal */
    nimcp_gpu_tensor_t* opc_growth;      /**< [N_opc] growth factors */
    nimcp_gpu_tensor_t* opc_diff_state;  /**< [N_opc] differentiation state */
    nimcp_gpu_tensor_t* opc_myelin_prod; /**< [N_opc] myelin production rate */

    uint32_t n_astrocytes;
    uint32_t n_microglia;
    uint32_t n_opcs;
    uint32_t n_neighbors;
} substrate_glial_tensors_t;

/**
 * @brief Metabolic tensor storage
 */
typedef struct {
    nimcp_gpu_tensor_t* atp_levels;      /**< [N_regions] ATP availability */
    nimcp_gpu_tensor_t* oxygen_levels;   /**< [N_regions] oxygen levels */
    nimcp_gpu_tensor_t* glucose_levels;  /**< [N_regions] glucose levels */
    nimcp_gpu_tensor_t* capacity;        /**< [N_regions] metabolic capacity */
    nimcp_gpu_tensor_t* fatigue;         /**< [N_regions] fatigue factors */
    nimcp_gpu_tensor_t* lactate_levels;  /**< [N_regions] lactate levels */
    nimcp_gpu_tensor_t* neural_activity; /**< [N_regions] activity levels */
    uint32_t n_regions;
} substrate_metabolic_tensors_t;

//=============================================================================
// Substrate GPU Context
//=============================================================================

/**
 * @brief Complete substrate GPU context
 */
struct substrate_gpu_context {
    nimcp_gpu_context_t* gpu_ctx;        /**< GPU context */
    substrate_gpu_config_t config;       /**< Configuration */

    // Tensor storage for each subsystem
    substrate_axon_tensors_t axon;
    substrate_dendrite_tensors_t dendrite;
    substrate_myelin_tensors_t myelin;
    substrate_neuromod_tensors_t neuromod;
    substrate_glial_tensors_t glial;
    substrate_metabolic_tensors_t metabolic;

    // Backend operations
    nimcp_substrate_ops_t* ops;          /**< Active operation functions */

    bool initialized;
};

//=============================================================================
// Context Management
//=============================================================================

/**
 * @brief Create substrate GPU context
 *
 * @param gpu_ctx GPU context (required)
 * @param config Configuration (NULL for defaults)
 * @return New substrate context or NULL on failure
 */
NIMCP_EXPORT substrate_gpu_context_t* substrate_gpu_create(
    nimcp_gpu_context_t* gpu_ctx,
    const substrate_gpu_config_t* config);

/**
 * @brief Destroy substrate GPU context
 */
NIMCP_EXPORT void substrate_gpu_destroy(substrate_gpu_context_t* ctx);

/**
 * @brief Initialize axon tensors
 *
 * @param ctx Substrate context
 * @param n_axons Number of axons
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_init_axons(substrate_gpu_context_t* ctx, uint32_t n_axons);

/**
 * @brief Initialize dendrite tensors
 *
 * @param ctx Substrate context
 * @param n_dendrites Number of dendrites
 * @param n_segments Segments per dendrite
 * @param n_spines Number of dendritic spines
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_init_dendrites(
    substrate_gpu_context_t* ctx,
    uint32_t n_dendrites,
    uint32_t n_segments,
    uint32_t n_spines);

/**
 * @brief Initialize myelin tensors
 *
 * @param ctx Substrate context
 * @param n_axons Number of myelinated axons
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_init_myelin(substrate_gpu_context_t* ctx, uint32_t n_axons);

/**
 * @brief Initialize neuromodulator tensors
 *
 * @param ctx Substrate context
 * @param n_pools Number of neuromodulator pools
 * @param n_types Number of neuromodulator types
 * @param n_synapses Number of modulated synapses
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_init_neuromod(
    substrate_gpu_context_t* ctx,
    uint32_t n_pools,
    uint32_t n_types,
    uint32_t n_synapses);

/**
 * @brief Initialize glial tensors
 *
 * @param ctx Substrate context
 * @param n_astrocytes Number of astrocytes
 * @param n_microglia Number of microglia
 * @param n_opcs Number of OPCs
 * @param n_neighbors Max gap junction neighbors
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_init_glial(
    substrate_gpu_context_t* ctx,
    uint32_t n_astrocytes,
    uint32_t n_microglia,
    uint32_t n_opcs,
    uint32_t n_neighbors);

/**
 * @brief Initialize metabolic tensors
 *
 * @param ctx Substrate context
 * @param n_regions Number of metabolic regions
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_init_metabolic(substrate_gpu_context_t* ctx, uint32_t n_regions);

//=============================================================================
// Axon Operations
//=============================================================================

/**
 * @brief Propagate signals along axons
 *
 * @param ctx Substrate context
 * @param input_signals Input spike signals (or NULL to use internal)
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_axon_propagate(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input_signals,
    float dt);

/**
 * @brief Update axon refractory states
 *
 * @param ctx Substrate context
 * @param spikes New spike events (or NULL to use internal)
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_axon_refractory(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* spikes,
    float dt);

/**
 * @brief Complete axon step (propagate + refractory)
 */
NIMCP_EXPORT int substrate_gpu_axon_step(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input_spikes,
    float dt);

//=============================================================================
// Dendrite Operations
//=============================================================================

/**
 * @brief Integrate dendrite cable equation
 *
 * @param ctx Substrate context
 * @param inputs Synaptic inputs [N_dend, N_seg] (or NULL for internal)
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_dendrite_cable(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* inputs,
    float dt);

/**
 * @brief Process NMDA spike detection
 *
 * @param ctx Substrate context
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_dendrite_nmda(substrate_gpu_context_t* ctx);

/**
 * @brief Update dendritic calcium dynamics
 *
 * @param ctx Substrate context
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_dendrite_calcium(substrate_gpu_context_t* ctx, float dt);

/**
 * @brief Propagate back-propagating action potentials
 *
 * @param ctx Substrate context
 * @param soma_spikes Somatic spikes (or NULL for internal)
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_dendrite_bap(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* soma_spikes,
    float dt);

/**
 * @brief Complete dendrite step (cable + NMDA + calcium + bAP)
 */
NIMCP_EXPORT int substrate_gpu_dendrite_step(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* inputs,
    const nimcp_gpu_tensor_t* soma_spikes,
    float dt);

//=============================================================================
// Myelin Operations
//=============================================================================

/**
 * @brief Compute G-ratios
 *
 * @param ctx Substrate context
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_myelin_g_ratio(substrate_gpu_context_t* ctx);

/**
 * @brief Calculate conduction velocities
 *
 * @param ctx Substrate context
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_myelin_velocity(substrate_gpu_context_t* ctx);

/**
 * @brief Update myelin plasticity
 *
 * @param ctx Substrate context
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_myelin_plasticity(substrate_gpu_context_t* ctx, float dt);

/**
 * @brief Complete myelin step (g-ratio + velocity + plasticity)
 */
NIMCP_EXPORT int substrate_gpu_myelin_step(substrate_gpu_context_t* ctx, float dt);

//=============================================================================
// Neuromodulator Operations
//=============================================================================

/**
 * @brief Decay neuromodulator concentrations
 *
 * @param ctx Substrate context
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_neuromod_decay(substrate_gpu_context_t* ctx, float dt);

/**
 * @brief Process neuromodulator release events
 *
 * @param ctx Substrate context
 * @param sites Release site indices
 * @param types Neuromodulator types
 * @param amounts Release amounts
 * @param n_events Number of release events
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_neuromod_release(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* sites,
    const nimcp_gpu_tensor_t* types,
    const nimcp_gpu_tensor_t* amounts,
    uint32_t n_events);

/**
 * @brief Compute synaptic modulation effects
 *
 * @param ctx Substrate context
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_neuromod_effect(substrate_gpu_context_t* ctx);

/**
 * @brief Update phasic-tonic dynamics
 *
 * @param ctx Substrate context
 * @param phasic_input Phasic burst signals (or NULL for internal)
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_neuromod_phasic_tonic(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* phasic_input,
    float dt);

/**
 * @brief Complete neuromodulator step (decay + phasic-tonic + effect)
 */
NIMCP_EXPORT int substrate_gpu_neuromod_step(substrate_gpu_context_t* ctx, float dt);

//=============================================================================
// Glial Operations
//=============================================================================

/**
 * @brief Propagate astrocyte calcium waves
 *
 * @param ctx Substrate context
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_astrocyte_calcium(substrate_gpu_context_t* ctx, float dt);

/**
 * @brief Process astrocyte gliotransmitter release
 *
 * @param ctx Substrate context
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_astrocyte_release(substrate_gpu_context_t* ctx);

/**
 * @brief Update microglia activation state
 *
 * @param ctx Substrate context
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_microglia_activation(substrate_gpu_context_t* ctx, float dt);

/**
 * @brief Update OPC differentiation
 *
 * @param ctx Substrate context
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_opc_differentiation(substrate_gpu_context_t* ctx, float dt);

/**
 * @brief Complete glial step (astrocyte + microglia + OPC)
 */
NIMCP_EXPORT int substrate_gpu_glial_step(substrate_gpu_context_t* ctx, float dt);

//=============================================================================
// Metabolic Operations
//=============================================================================

/**
 * @brief Compute metabolic capacity and fatigue
 *
 * @param ctx Substrate context
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_metabolic_effects(substrate_gpu_context_t* ctx);

/**
 * @brief Update metabolic state (ATP, lactate)
 *
 * @param ctx Substrate context
 * @param neural_activity Activity levels (or NULL for internal)
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_metabolic_update(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* neural_activity,
    float dt);

/**
 * @brief Complete metabolic step (effects + update)
 */
NIMCP_EXPORT int substrate_gpu_metabolic_step(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* neural_activity,
    float dt);

//=============================================================================
// Unified Step
//=============================================================================

/**
 * @brief Execute complete substrate simulation step
 *
 * Processes all subsystems in order:
 * 1. Axon propagation and refractory
 * 2. Dendrite cable, NMDA, calcium, bAP
 * 3. Myelin G-ratio, velocity, plasticity
 * 4. Neuromodulator decay, phasic-tonic, effect
 * 5. Glial astrocyte, microglia, OPC
 * 6. Metabolic effects and update
 *
 * @param ctx Substrate context
 * @param input_spikes Input spike signals
 * @param synaptic_inputs Synaptic inputs to dendrites
 * @param neural_activity Overall activity levels
 * @param dt Time step
 * @return 0 on success
 */
NIMCP_EXPORT int substrate_gpu_full_step(
    substrate_gpu_context_t* ctx,
    const nimcp_gpu_tensor_t* input_spikes,
    const nimcp_gpu_tensor_t* synaptic_inputs,
    const nimcp_gpu_tensor_t* neural_activity,
    float dt);

//=============================================================================
// Tensor Access (for integration with other modules)
//=============================================================================

/**
 * @brief Get axon tensor storage
 */
NIMCP_EXPORT substrate_axon_tensors_t* substrate_gpu_get_axon_tensors(substrate_gpu_context_t* ctx);

/**
 * @brief Get dendrite tensor storage
 */
NIMCP_EXPORT substrate_dendrite_tensors_t* substrate_gpu_get_dendrite_tensors(substrate_gpu_context_t* ctx);

/**
 * @brief Get myelin tensor storage
 */
NIMCP_EXPORT substrate_myelin_tensors_t* substrate_gpu_get_myelin_tensors(substrate_gpu_context_t* ctx);

/**
 * @brief Get neuromodulator tensor storage
 */
NIMCP_EXPORT substrate_neuromod_tensors_t* substrate_gpu_get_neuromod_tensors(substrate_gpu_context_t* ctx);

/**
 * @brief Get glial tensor storage
 */
NIMCP_EXPORT substrate_glial_tensors_t* substrate_gpu_get_glial_tensors(substrate_gpu_context_t* ctx);

/**
 * @brief Get metabolic tensor storage
 */
NIMCP_EXPORT substrate_metabolic_tensors_t* substrate_gpu_get_metabolic_tensors(substrate_gpu_context_t* ctx);

//=============================================================================
// Convenience Macros
//=============================================================================

/**
 * @brief Get substrate operations from kernel backend
 */
#define NIMCP_SUBSTRATE_OPS() (&nimcp_get_kernel_backend()->substrate)

/**
 * @brief Call substrate operation with error handling
 */
#define NIMCP_CALL_SUBSTRATE_OP(op, ...) \
    (NIMCP_SUBSTRATE_OPS()->op ? NIMCP_SUBSTRATE_OPS()->op(__VA_ARGS__) : NIMCP_KERNEL_ERROR_NOT_IMPLEMENTED)

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SUBSTRATE_GPU_H
