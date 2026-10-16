/**
 * @file nimcp_neuromodulator_gpu.h
 * @brief GPU-accelerated Neuromodulator Dynamics Kernels
 *
 * WHAT: CUDA kernels for neuromodulator system computations
 * WHY:  GPU acceleration for biologically-accurate neuromodulator dynamics
 * HOW:  Custom kernels for dopamine, serotonin, acetylcholine, norepinephrine
 *
 * ARCHITECTURE:
 * - Dopamine: Reward prediction, motivation, motor control
 * - Serotonin: Mood regulation, impulse control, satiety
 * - Acetylcholine: Attention, learning modulation, memory
 * - Norepinephrine: Arousal, vigilance, stress response
 * - Receptor kinetics: Binding/unbinding, desensitization
 * - Release/Reuptake: Vesicle release, transporter dynamics
 *
 * All functions support both CUDA GPU and CPU fallback implementations.
 *
 * @version 1.0
 * @author NIMCP Development Team
 * @date 2025
 */

#ifndef NIMCP_NEUROMODULATOR_GPU_H
#define NIMCP_NEUROMODULATOR_GPU_H

#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"
#include "common/nimcp_export.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Neuromodulator Types
//=============================================================================

/**
 * @brief Neuromodulator type enumeration
 */
typedef enum {
    NIMCP_NEUROMOD_DOPAMINE = 0,      /**< Dopamine (DA) */
    NIMCP_NEUROMOD_SEROTONIN = 1,     /**< Serotonin (5-HT) */
    NIMCP_NEUROMOD_ACETYLCHOLINE = 2, /**< Acetylcholine (ACh) */
    NIMCP_NEUROMOD_NOREPINEPHRINE = 3,/**< Norepinephrine (NE) */
    NIMCP_NEUROMOD_GABA = 4,          /**< GABA (inhibitory) */
    NIMCP_NEUROMOD_GLUTAMATE = 5,     /**< Glutamate (excitatory) */
    NIMCP_NEUROMOD_COUNT = 6
} nimcp_neuromod_type_t;

//=============================================================================
// Dopamine Parameters and Structures
//=============================================================================

/**
 * @brief Dopamine system parameters for GPU
 */
typedef struct {
    float baseline;           /**< Baseline DA concentration (uM) */
    float release_rate;       /**< DA release rate per spike */
    float reuptake_tau;       /**< Reuptake time constant (ms) */
    float decay_tau;          /**< Extracellular decay (ms) */
    float d1_affinity;        /**< D1 receptor affinity (Kd, uM) */
    float d2_affinity;        /**< D2 receptor affinity (Kd, uM) */
    float max_conc;           /**< Maximum DA concentration */
    float burst_factor;       /**< Burst firing amplification */
    float tonic_rate;         /**< Tonic firing rate (Hz) */
    float phasic_amplitude;   /**< Phasic burst amplitude */
} nimcp_gpu_dopamine_params_t;

/**
 * @brief Dopamine neuron state for GPU batch processing
 */
typedef struct {
    nimcp_gpu_tensor_t* concentration;    /**< Extracellular DA concentration */
    nimcp_gpu_tensor_t* d1_occupancy;     /**< D1 receptor occupancy (0-1) */
    nimcp_gpu_tensor_t* d2_occupancy;     /**< D2 receptor occupancy (0-1) */
    nimcp_gpu_tensor_t* vesicle_pool;     /**< Available vesicle pool */
    nimcp_gpu_tensor_t* release_prob;     /**< Release probability */
    nimcp_gpu_tensor_t* reward_prediction;/**< Reward prediction error */
    size_t n_neurons;                     /**< Number of DA neurons */
    size_t n_targets;                     /**< Number of target sites */
} nimcp_gpu_dopamine_state_t;

//=============================================================================
// Serotonin Parameters and Structures
//=============================================================================

/**
 * @brief Serotonin system parameters for GPU
 */
typedef struct {
    float baseline;           /**< Baseline 5-HT concentration (uM) */
    float release_rate;       /**< 5-HT release rate per spike */
    float reuptake_tau;       /**< Reuptake time constant (ms) */
    float decay_tau;          /**< Extracellular decay (ms) */
    float ht1a_affinity;      /**< 5-HT1A receptor affinity (Kd) */
    float ht2a_affinity;      /**< 5-HT2A receptor affinity (Kd) */
    float max_conc;           /**< Maximum 5-HT concentration */
    float autoreceptor_gain;  /**< Autoreceptor feedback gain */
    float synthesis_rate;     /**< Tryptophan hydroxylase rate */
} nimcp_gpu_serotonin_params_t;

/**
 * @brief Serotonin state for GPU batch processing
 */
typedef struct {
    nimcp_gpu_tensor_t* concentration;    /**< Extracellular 5-HT concentration */
    nimcp_gpu_tensor_t* ht1a_occupancy;   /**< 5-HT1A receptor occupancy */
    nimcp_gpu_tensor_t* ht2a_occupancy;   /**< 5-HT2A receptor occupancy */
    nimcp_gpu_tensor_t* vesicle_pool;     /**< Available vesicle pool */
    nimcp_gpu_tensor_t* synthesis_state;  /**< Synthesis enzyme state */
    size_t n_neurons;                     /**< Number of 5-HT neurons */
    size_t n_targets;                     /**< Number of target sites */
} nimcp_gpu_serotonin_state_t;

//=============================================================================
// Acetylcholine Parameters and Structures
//=============================================================================

/**
 * @brief Acetylcholine system parameters for GPU
 */
typedef struct {
    float baseline;           /**< Baseline ACh concentration (uM) */
    float release_rate;       /**< ACh release rate per spike */
    float ache_rate;          /**< Acetylcholinesterase hydrolysis rate */
    float decay_tau;          /**< Diffusion decay (ms) */
    float m1_affinity;        /**< M1 muscarinic affinity (Kd) */
    float m2_affinity;        /**< M2 muscarinic affinity (Kd) */
    float nicotinic_affinity; /**< Nicotinic receptor affinity (Kd) */
    float max_conc;           /**< Maximum ACh concentration */
    float desensitization_tau;/**< Receptor desensitization (ms) */
    float choline_uptake_km;  /**< Choline reuptake Km */
} nimcp_gpu_acetylcholine_params_t;

/**
 * @brief Acetylcholine state for GPU batch processing
 */
typedef struct {
    nimcp_gpu_tensor_t* concentration;    /**< Extracellular ACh concentration */
    nimcp_gpu_tensor_t* m1_occupancy;     /**< M1 muscarinic occupancy */
    nimcp_gpu_tensor_t* m2_occupancy;     /**< M2 muscarinic occupancy */
    nimcp_gpu_tensor_t* nicotinic_state;  /**< Nicotinic receptor state */
    nimcp_gpu_tensor_t* vesicle_pool;     /**< Available vesicle pool */
    nimcp_gpu_tensor_t* attention_signal; /**< Attention modulation output */
    size_t n_neurons;                     /**< Number of cholinergic neurons */
    size_t n_targets;                     /**< Number of target sites */
} nimcp_gpu_acetylcholine_state_t;

//=============================================================================
// Norepinephrine Parameters and Structures
//=============================================================================

/**
 * @brief Norepinephrine system parameters for GPU
 */
typedef struct {
    float baseline;           /**< Baseline NE concentration (uM) */
    float release_rate;       /**< NE release rate per spike */
    float reuptake_tau;       /**< NET reuptake time constant (ms) */
    float decay_tau;          /**< Extracellular decay (ms) */
    float alpha1_affinity;    /**< Alpha-1 receptor affinity (Kd) */
    float alpha2_affinity;    /**< Alpha-2 receptor affinity (Kd) */
    float beta_affinity;      /**< Beta receptor affinity (Kd) */
    float max_conc;           /**< Maximum NE concentration */
    float lc_baseline_rate;   /**< LC baseline firing rate (Hz) */
    float stress_gain;        /**< Stress response amplification */
} nimcp_gpu_norepinephrine_params_t;

/**
 * @brief Norepinephrine state for GPU batch processing
 */
typedef struct {
    nimcp_gpu_tensor_t* concentration;    /**< Extracellular NE concentration */
    nimcp_gpu_tensor_t* alpha1_occupancy; /**< Alpha-1 receptor occupancy */
    nimcp_gpu_tensor_t* alpha2_occupancy; /**< Alpha-2 receptor occupancy */
    nimcp_gpu_tensor_t* beta_occupancy;   /**< Beta receptor occupancy */
    nimcp_gpu_tensor_t* vesicle_pool;     /**< Available vesicle pool */
    nimcp_gpu_tensor_t* arousal_signal;   /**< Arousal modulation output */
    size_t n_neurons;                     /**< Number of LC neurons */
    size_t n_targets;                     /**< Number of target sites */
} nimcp_gpu_norepinephrine_state_t;

//=============================================================================
// Unified Neuromodulator State
//=============================================================================

/**
 * @brief Unified neuromodulator system state
 */
typedef struct {
    nimcp_gpu_dopamine_state_t* dopamine;
    nimcp_gpu_serotonin_state_t* serotonin;
    nimcp_gpu_acetylcholine_state_t* acetylcholine;
    nimcp_gpu_norepinephrine_state_t* norepinephrine;
    nimcp_gpu_tensor_t* interaction_matrix; /**< Cross-modulator interactions */
    float dt;                               /**< Simulation timestep (ms) */
} nimcp_gpu_neuromod_system_t;

//=============================================================================
// Default Parameter Functions
//=============================================================================

/**
 * @brief Get default dopamine parameters
 */
NIMCP_EXPORT nimcp_gpu_dopamine_params_t nimcp_gpu_dopamine_params_default(void);

/**
 * @brief Get default serotonin parameters
 */
NIMCP_EXPORT nimcp_gpu_serotonin_params_t nimcp_gpu_serotonin_params_default(void);

/**
 * @brief Get default acetylcholine parameters
 */
NIMCP_EXPORT nimcp_gpu_acetylcholine_params_t nimcp_gpu_acetylcholine_params_default(void);

/**
 * @brief Get default norepinephrine parameters
 */
NIMCP_EXPORT nimcp_gpu_norepinephrine_params_t nimcp_gpu_norepinephrine_params_default(void);

//=============================================================================
// Dopamine Kernel Functions
//=============================================================================

/**
 * @brief Update dopamine concentration based on neural activity
 *
 * @param ctx GPU context
 * @param state Dopamine state to update
 * @param spikes Input spike tensor from DA neurons
 * @param dt Time step (ms)
 * @param params Dopamine parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_dopamine_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_dopamine_state_t* state,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_dopamine_params_t* params);

/**
 * @brief Compute reward prediction error (RPE)
 *
 * @param ctx GPU context
 * @param state Dopamine state
 * @param reward Actual reward signal
 * @param predicted Predicted reward
 * @param rpe_out Output RPE tensor
 * @param params Dopamine parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_dopamine_compute_rpe(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_dopamine_state_t* state,
    const nimcp_gpu_tensor_t* reward,
    const nimcp_gpu_tensor_t* predicted,
    nimcp_gpu_tensor_t* rpe_out,
    const nimcp_gpu_dopamine_params_t* params);

/**
 * @brief Update D1/D2 receptor occupancy
 *
 * @param ctx GPU context
 * @param state Dopamine state to update
 * @param dt Time step (ms)
 * @param params Dopamine parameters
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_dopamine_receptor_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_dopamine_state_t* state,
    float dt,
    const nimcp_gpu_dopamine_params_t* params);

/**
 * @brief Apply dopamine modulation to synaptic weights
 *
 * @param ctx GPU context
 * @param weights Synaptic weights to modulate
 * @param d1_effect D1 receptor effect tensor
 * @param d2_effect D2 receptor effect tensor
 * @param eligibility Eligibility traces
 * @param learning_rate Base learning rate
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_dopamine_modulate_plasticity(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* weights,
    const nimcp_gpu_tensor_t* d1_effect,
    const nimcp_gpu_tensor_t* d2_effect,
    const nimcp_gpu_tensor_t* eligibility,
    float learning_rate);

//=============================================================================
// Serotonin Kernel Functions
//=============================================================================

/**
 * @brief Update serotonin concentration based on neural activity
 */
NIMCP_EXPORT bool nimcp_gpu_serotonin_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_serotonin_state_t* state,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_serotonin_params_t* params);

/**
 * @brief Update 5-HT receptor occupancy
 */
NIMCP_EXPORT bool nimcp_gpu_serotonin_receptor_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_serotonin_state_t* state,
    float dt,
    const nimcp_gpu_serotonin_params_t* params);

/**
 * @brief Compute serotonergic mood/impulse modulation
 */
NIMCP_EXPORT bool nimcp_gpu_serotonin_modulate_behavior(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_serotonin_state_t* state,
    nimcp_gpu_tensor_t* impulse_control,
    nimcp_gpu_tensor_t* mood_signal,
    const nimcp_gpu_serotonin_params_t* params);

//=============================================================================
// Acetylcholine Kernel Functions
//=============================================================================

/**
 * @brief Update acetylcholine concentration based on neural activity
 */
NIMCP_EXPORT bool nimcp_gpu_acetylcholine_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acetylcholine_state_t* state,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_acetylcholine_params_t* params);

/**
 * @brief Update muscarinic/nicotinic receptor states
 */
NIMCP_EXPORT bool nimcp_gpu_acetylcholine_receptor_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acetylcholine_state_t* state,
    float dt,
    const nimcp_gpu_acetylcholine_params_t* params);

/**
 * @brief Compute attention modulation signal
 */
NIMCP_EXPORT bool nimcp_gpu_acetylcholine_compute_attention(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_acetylcholine_state_t* state,
    const nimcp_gpu_tensor_t* salience,
    nimcp_gpu_tensor_t* attention_out,
    const nimcp_gpu_acetylcholine_params_t* params);

/**
 * @brief Modulate learning rate based on ACh levels
 */
NIMCP_EXPORT bool nimcp_gpu_acetylcholine_modulate_learning(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* learning_rates,
    const nimcp_gpu_tensor_t* ach_concentration,
    float baseline_rate,
    float max_modulation);

//=============================================================================
// Norepinephrine Kernel Functions
//=============================================================================

/**
 * @brief Update norepinephrine concentration based on LC activity
 */
NIMCP_EXPORT bool nimcp_gpu_norepinephrine_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_norepinephrine_state_t* state,
    const nimcp_gpu_tensor_t* spikes,
    float dt,
    const nimcp_gpu_norepinephrine_params_t* params);

/**
 * @brief Update adrenergic receptor occupancy
 */
NIMCP_EXPORT bool nimcp_gpu_norepinephrine_receptor_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_norepinephrine_state_t* state,
    float dt,
    const nimcp_gpu_norepinephrine_params_t* params);

/**
 * @brief Compute arousal/vigilance signal
 */
NIMCP_EXPORT bool nimcp_gpu_norepinephrine_compute_arousal(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_norepinephrine_state_t* state,
    const nimcp_gpu_tensor_t* stress_input,
    nimcp_gpu_tensor_t* arousal_out,
    const nimcp_gpu_norepinephrine_params_t* params);

/**
 * @brief Modulate neural gain based on NE levels (gain modulation)
 */
NIMCP_EXPORT bool nimcp_gpu_norepinephrine_modulate_gain(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* neural_gains,
    const nimcp_gpu_tensor_t* ne_concentration,
    float optimal_arousal,
    float gain_sensitivity);

//=============================================================================
// Integrated Neuromodulator Functions
//=============================================================================

/**
 * @brief Update all neuromodulator systems in one step
 */
NIMCP_EXPORT bool nimcp_gpu_neuromod_system_update(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_neuromod_system_t* system,
    const nimcp_gpu_tensor_t* da_spikes,
    const nimcp_gpu_tensor_t* ht_spikes,
    const nimcp_gpu_tensor_t* ach_spikes,
    const nimcp_gpu_tensor_t* ne_spikes,
    float dt);

/**
 * @brief Compute cross-modulator interactions
 *
 * Models how neuromodulators affect each other's release/effects
 */
NIMCP_EXPORT bool nimcp_gpu_neuromod_interactions(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_neuromod_system_t* system);

/**
 * @brief Compute combined neuromodulator effect on target
 *
 * @param ctx GPU context
 * @param system Neuromodulator system state
 * @param target_activity Target neuron activity
 * @param modulated_output Output modulated activity
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_neuromod_apply_combined(
    nimcp_gpu_context_t* ctx,
    const nimcp_gpu_neuromod_system_t* system,
    const nimcp_gpu_tensor_t* target_activity,
    nimcp_gpu_tensor_t* modulated_output);

//=============================================================================
// Vesicle Dynamics
//=============================================================================

/**
 * @brief Update vesicle pool dynamics (release and replenishment)
 *
 * @param ctx GPU context
 * @param vesicle_pool Current vesicle pool tensor
 * @param spikes Input spikes triggering release
 * @param release_prob Release probability per spike
 * @param replenish_rate Vesicle replenishment rate
 * @param max_pool Maximum vesicle pool size
 * @param dt Time step
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_vesicle_dynamics(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* vesicle_pool,
    const nimcp_gpu_tensor_t* spikes,
    float release_prob,
    float replenish_rate,
    float max_pool,
    float dt);

/**
 * @brief Compute receptor binding kinetics (generic)
 *
 * @param ctx GPU context
 * @param occupancy Receptor occupancy to update
 * @param concentration Ligand concentration
 * @param affinity Receptor affinity (Kd)
 * @param on_rate Binding rate constant
 * @param off_rate Unbinding rate constant
 * @param dt Time step
 * @return true on success
 */
NIMCP_EXPORT bool nimcp_gpu_receptor_kinetics(
    nimcp_gpu_context_t* ctx,
    nimcp_gpu_tensor_t* occupancy,
    const nimcp_gpu_tensor_t* concentration,
    float affinity,
    float on_rate,
    float off_rate,
    float dt);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEUROMODULATOR_GPU_H
