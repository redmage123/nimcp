//=============================================================================
// nimcp_hh_quantum_bridge.h - Hodgkin-Huxley Quantum Monte Carlo Bridge
//=============================================================================
/**
 * @file nimcp_hh_quantum_bridge.h
 * @brief QMC integration for Hodgkin-Huxley neuron model
 *
 * WHAT: Quantum Monte Carlo methods for HH neuron optimization and analysis
 *
 * WHY:  QMC provides:
 *       - Parameter optimization via adaptive annealing
 *       - Ion channel stochastic noise simulation
 *       - Information-theoretic analysis of spike patterns
 *       - Entropy estimation for neural coding
 *
 * HOW:  - Uses qmc_adaptive_anneal() for conductance/reversal optimization
 *       - Uses qmc_estimate_entropy() for spike train entropy
 *       - Integrates with HH neuron API for parameter updates
 *
 * BIOLOGICAL: Ion channels exhibit stochastic gating at small scales.
 * QMC methods help capture this stochasticity and optimize parameters
 * to match target firing patterns or physiological measurements.
 *
 * @author NIMCP Development Team
 * @date 2026-01-12
 * @version 1.0.0
 */

#ifndef NIMCP_HH_QUANTUM_BRIDGE_H
#define NIMCP_HH_QUANTUM_BRIDGE_H

#include <stdbool.h>
#include <stdint.h>
#include "common/nimcp_export.h"
#include "physics/biophysics/nimcp_hodgkin_huxley.h"
#include "utils/quantum/nimcp_quantum_monte_carlo.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default number of annealing iterations for HH optimization */
#define HH_QMC_DEFAULT_ITERATIONS       1000

/** Default initial temperature for annealing */
#define HH_QMC_DEFAULT_INITIAL_TEMP     10.0f

/** Default final temperature for annealing */
#define HH_QMC_DEFAULT_FINAL_TEMP       0.01f

/** Default quantum tunneling strength */
#define HH_QMC_DEFAULT_QUANTUM_STRENGTH 0.3f

/** Number of parameters for basic HH optimization */
#define HH_QMC_PARAM_DIM_BASIC          4   /**< g_Na, g_K, E_Na, E_K */

/** Number of parameters for extended HH optimization */
#define HH_QMC_PARAM_DIM_EXTENDED       8   /**< Basic + g_Ca_L, E_Ca, g_L, E_L */

//=============================================================================
// Target Types for Optimization
//=============================================================================

/**
 * @brief Target firing behavior for optimization
 */
typedef struct {
    float target_firing_rate;       /**< Desired firing rate (Hz) */
    float target_threshold;         /**< Desired threshold current (uA/cm^2) */
    float target_spike_width;       /**< Desired spike width at half-max (ms) */
    float target_rheobase;          /**< Desired rheobase (uA/cm^2) */

    /* Weight factors for multi-objective optimization */
    float weight_rate;              /**< Weight for firing rate error */
    float weight_threshold;         /**< Weight for threshold error */
    float weight_spike_width;       /**< Weight for spike width error */
    float weight_rheobase;          /**< Weight for rheobase error */
} hh_qmc_target_t;

/**
 * @brief Configuration for HH QMC optimization
 */
typedef struct {
    /** Annealing parameters */
    float initial_temp;
    float final_temp;
    uint32_t num_iterations;
    float quantum_strength;

    /** Optimization bounds */
    float g_Na_min, g_Na_max;       /**< Sodium conductance bounds */
    float g_K_min, g_K_max;         /**< Potassium conductance bounds */
    float E_Na_min, E_Na_max;       /**< Sodium reversal bounds */
    float E_K_min, E_K_max;         /**< Potassium reversal bounds */

    /** Extended parameters (if optimize_extended = true) */
    bool optimize_extended;
    float g_Ca_min, g_Ca_max;
    float E_Ca_min, E_Ca_max;
    float g_L_min, g_L_max;
    float E_L_min, E_L_max;

    /** Simulation parameters for evaluation */
    float eval_duration_ms;         /**< Duration for firing rate evaluation */
    float eval_dt;                  /**< Timestep for evaluation */
    float eval_current;             /**< Current for evaluation */

    /** Random seed */
    uint32_t seed;
} hh_qmc_config_t;

/**
 * @brief Result of HH QMC optimization
 */
typedef struct {
    /** Optimized parameters */
    float opt_g_Na;
    float opt_g_K;
    float opt_E_Na;
    float opt_E_K;

    /* Extended parameters (if requested) */
    float opt_g_Ca;
    float opt_E_Ca;
    float opt_g_L;
    float opt_E_L;

    /** Achieved firing behavior */
    float achieved_firing_rate;
    float achieved_threshold;
    float achieved_spike_width;
    float achieved_rheobase;

    /** Optimization statistics */
    float final_energy;
    float acceptance_rate;
    uint32_t iterations_run;
    uint32_t tunneling_events;

    /** Error metrics */
    float rate_error;
    float threshold_error;
    float spike_width_error;
    float rheobase_error;
    float total_error;

    /** Success flag */
    bool converged;
} hh_qmc_result_t;

//=============================================================================
// Entropy Analysis Types
//=============================================================================

/**
 * @brief Configuration for spike train entropy analysis
 */
typedef struct {
    uint32_t num_samples;           /**< Number of MC samples */
    float bin_width_ms;             /**< Temporal bin width for ISI */
    uint32_t num_bins;              /**< Number of ISI bins */
    bool use_stratified;            /**< Use stratified sampling */
    uint32_t seed;
} hh_entropy_config_t;

/**
 * @brief Result of spike train entropy analysis
 */
typedef struct {
    float isi_entropy;              /**< Entropy of inter-spike intervals */
    float spike_count_entropy;      /**< Entropy of spike counts */
    float mutual_info;              /**< Mutual info: stimulus -> response */
    float coding_efficiency;        /**< Bits per spike */
    float variance;
    float std_error;
} hh_entropy_result_t;

//=============================================================================
// Stochastic Channel Types
//=============================================================================

/**
 * @brief Configuration for stochastic channel simulation
 */
typedef struct {
    uint32_t num_channels;          /**< Number of ion channels per type */
    float channel_conductance;      /**< Single channel conductance (pS) */
    bool simulate_na;               /**< Simulate Na channel noise */
    bool simulate_k;                /**< Simulate K channel noise */
    uint32_t num_trajectories;      /**< MC trajectories for variance est */
    uint32_t seed;
} hh_stochastic_config_t;

/**
 * @brief Result of stochastic channel simulation
 */
typedef struct {
    float voltage_variance;         /**< Variance in membrane voltage */
    float spike_time_jitter;        /**< Spike timing variance (ms) */
    float channel_noise_power;      /**< Power of channel noise */
    float snr;                      /**< Signal-to-noise ratio */
} hh_stochastic_result_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default QMC optimization configuration
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_default_config(hh_qmc_config_t* config);

/**
 * @brief Get default target firing behavior
 *
 * @param target    Target to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_default_target(hh_qmc_target_t* target);

/**
 * @brief Get default entropy analysis configuration
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_entropy_default_config(hh_entropy_config_t* config);

/**
 * @brief Get default stochastic channel configuration
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_stochastic_default_config(hh_stochastic_config_t* config);

//=============================================================================
// Parameter Optimization API
//=============================================================================

/**
 * @brief Optimize HH neuron parameters via QMC annealing
 *
 * WHAT: Find optimal conductances/reversal potentials to match target behavior
 * WHY:  Fit models to experimental data or achieve desired firing patterns
 * HOW:  Uses qmc_adaptive_anneal() with custom energy function
 *
 * @param neuron    Neuron whose parameters will be optimized (modified in place)
 * @param target    Target firing behavior
 * @param config    Optimization configuration (NULL for defaults)
 * @param result    Output optimization result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_optimize_parameters(
    nimcp_hh_neuron_t* neuron,
    const hh_qmc_target_t* target,
    const hh_qmc_config_t* config,
    hh_qmc_result_t* result
);

/**
 * @brief Optimize HH neuron to match experimental f-I curve
 *
 * @param neuron        Neuron to optimize
 * @param currents      Array of test currents
 * @param target_rates  Target firing rates for each current
 * @param num_points    Number of f-I curve points
 * @param config        Optimization configuration
 * @param result        Output result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_fit_fi_curve(
    nimcp_hh_neuron_t* neuron,
    const float* currents,
    const float* target_rates,
    uint32_t num_points,
    const hh_qmc_config_t* config,
    hh_qmc_result_t* result
);

/**
 * @brief Apply optimized parameters to neuron
 *
 * @param neuron    Neuron to update
 * @param result    Optimization result containing parameters
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_apply_result(
    nimcp_hh_neuron_t* neuron,
    const hh_qmc_result_t* result
);

//=============================================================================
// Entropy Analysis API
//=============================================================================

/**
 * @brief Estimate entropy of spike train
 *
 * WHAT: Compute information-theoretic measures of spike trains
 * WHY:  Quantify neural coding efficiency and variability
 * HOW:  Uses qmc_estimate_entropy() on ISI distribution
 *
 * @param neuron        Neuron to analyze
 * @param stimulus      Input current trajectory
 * @param stimulus_len  Length of stimulus (timesteps)
 * @param dt            Timestep (ms)
 * @param config        Entropy estimation configuration
 * @param result        Output entropy result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_spike_train_entropy(
    nimcp_hh_neuron_t* neuron,
    const float* stimulus,
    uint32_t stimulus_len,
    float dt,
    const hh_entropy_config_t* config,
    hh_entropy_result_t* result
);

/**
 * @brief Estimate mutual information between stimulus and response
 *
 * @param neuron        Neuron to analyze
 * @param stimuli       Array of stimulus trajectories
 * @param num_stimuli   Number of different stimuli
 * @param stim_len      Length of each stimulus
 * @param num_trials    Trials per stimulus
 * @param dt            Timestep (ms)
 * @param config        Configuration
 * @param mutual_info   Output: mutual information (bits)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_mutual_information(
    nimcp_hh_neuron_t* neuron,
    const float** stimuli,
    uint32_t num_stimuli,
    uint32_t stim_len,
    uint32_t num_trials,
    float dt,
    const hh_entropy_config_t* config,
    float* mutual_info
);

//=============================================================================
// Stochastic Channel Simulation API
//=============================================================================

/**
 * @brief Simulate stochastic ion channel gating
 *
 * WHAT: Add realistic channel noise to HH model
 * WHY:  Small channels have significant stochastic effects
 * HOW:  MC sampling of channel state transitions
 *
 * @param neuron        Neuron to simulate
 * @param I_ext         External current
 * @param duration_ms   Simulation duration
 * @param dt            Timestep
 * @param config        Stochastic configuration
 * @param result        Output stochastic result
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_stochastic_simulation(
    nimcp_hh_neuron_t* neuron,
    float I_ext,
    float duration_ms,
    float dt,
    const hh_stochastic_config_t* config,
    hh_stochastic_result_t* result
);

/**
 * @brief Compute voltage variance from channel noise
 *
 * @param neuron        Neuron to analyze
 * @param I_ext         External current
 * @param config        Stochastic configuration
 * @param variance      Output: voltage variance (mV^2)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_channel_noise_variance(
    nimcp_hh_neuron_t* neuron,
    float I_ext,
    const hh_stochastic_config_t* config,
    float* variance
);

//=============================================================================
// Population Analysis API
//=============================================================================

/**
 * @brief Analyze population synchrony via QMC
 *
 * Uses QMC to estimate phase coherence across population
 *
 * @param population    Population to analyze
 * @param coherence     Output: phase coherence [0,1]
 * @param entropy       Output: population entropy
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_qmc_population_coherence(
    const nimcp_hh_population_t* population,
    float* coherence,
    float* entropy
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_QUANTUM_BRIDGE_H */
