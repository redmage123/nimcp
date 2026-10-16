/**
 * @file nimcp_neural_constants.h
 * @brief Centralized neural simulation constants for NIMCP
 * @version 1.0.0
 * @date 2026-02-15
 *
 * WHAT: Defines all neural/biophysical simulation constants
 * WHY:  Eliminates magic numbers, ensures consistent neural parameters
 * HOW:  Single header with hierarchical organization by biophysical domain
 *
 * Usage: #include "constants/nimcp_neural_constants.h"
 */

#ifndef NIMCP_NEURAL_CONSTANTS_H
#define NIMCP_NEURAL_CONSTANTS_H

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Membrane Potential Constants (millivolts)
 *===========================================================================*/

/** @brief Resting membrane potential (mV) */
#define NIMCP_RESTING_POTENTIAL_MV            -70.0f

/** @brief Firing/spike threshold potential (mV) */
#define NIMCP_FIRING_THRESHOLD_MV             -55.0f

/** @brief Reset potential after spike (mV) */
#define NIMCP_RESET_POTENTIAL_MV              -65.0f

/** @brief Excitatory reversal potential (mV) */
#define NIMCP_EXCITATORY_REVERSAL_MV          0.0f

/** @brief Inhibitory reversal potential (mV) */
#define NIMCP_INHIBITORY_REVERSAL_MV          -80.0f

/*=============================================================================
 * Time Constants (milliseconds)
 *===========================================================================*/

/** @brief Membrane time constant (ms) */
#define NIMCP_MEMBRANE_TAU_MS                 20.0f

/** @brief Synaptic time constant - excitatory (ms) */
#define NIMCP_SYNAPTIC_TAU_EXCITATORY_MS      5.0f

/** @brief Synaptic time constant - inhibitory (ms) */
#define NIMCP_SYNAPTIC_TAU_INHIBITORY_MS      10.0f

/** @brief Absolute refractory period (ms) */
#define NIMCP_REFRACTORY_PERIOD_MS            2.0f

/** @brief Default simulation time step (ms) */
#define NIMCP_SIMULATION_DT_MS                1.0f

/** @brief Fine simulation time step (ms) */
#define NIMCP_SIMULATION_DT_FINE_MS           0.1f

/*=============================================================================
 * Neuromodulator Baseline Levels
 *===========================================================================*/

/** @brief Default dopamine baseline level */
#define NIMCP_DOPAMINE_BASELINE               0.5f

/** @brief Default serotonin baseline level */
#define NIMCP_SEROTONIN_BASELINE              0.5f

/** @brief Default norepinephrine baseline level */
#define NIMCP_NOREPINEPHRINE_BASELINE         0.5f

/** @brief Default acetylcholine baseline level */
#define NIMCP_ACETYLCHOLINE_BASELINE          0.5f

/** @brief Default GABA baseline level */
#define NIMCP_GABA_BASELINE                   0.5f

/** @brief Default glutamate baseline level */
#define NIMCP_GLUTAMATE_BASELINE              0.5f

/*=============================================================================
 * Synaptic Weight Constants
 *===========================================================================*/

/** @brief Default initial synaptic weight */
#define NIMCP_SYNAPSE_WEIGHT_INIT             0.5f

/** @brief Maximum synaptic weight */
#define NIMCP_SYNAPSE_WEIGHT_MAX              1.0f

/** @brief Minimum synaptic weight */
#define NIMCP_SYNAPSE_WEIGHT_MIN              0.0f

/** @brief Default connection probability for random networks */
#define NIMCP_CONNECTION_PROBABILITY           0.1f

/*=============================================================================
 * Firing Rate Constants
 *===========================================================================*/

/** @brief Default target firing rate (Hz) */
#define NIMCP_TARGET_FIRING_RATE_HZ           10.0f

/** @brief Maximum firing rate (Hz) */
#define NIMCP_MAX_FIRING_RATE_HZ              200.0f

/** @brief Default adaptation rate */
#define NIMCP_ADAPTATION_RATE_DEFAULT          0.01f

/*=============================================================================
 * LIF/Izhikevich Model Constants
 *===========================================================================*/

/** @brief Default leak conductance */
#define NIMCP_LEAK_CONDUCTANCE_DEFAULT         0.05f

/** @brief Default membrane capacitance (nF) */
#define NIMCP_MEMBRANE_CAPACITANCE_NF          1.0f

/** @brief Default noise amplitude for stochastic neurons */
#define NIMCP_NEURAL_NOISE_AMPLITUDE           0.1f

/*=============================================================================
 * Oscillation Constants
 *===========================================================================*/

/** @brief Default oscillation coupling strength */
#define NIMCP_OSCILLATION_COUPLING_DEFAULT     0.1f

/** @brief Default phase coherence threshold */
#define NIMCP_PHASE_COHERENCE_THRESHOLD        0.7f

/** @brief Default natural frequency (Hz) for Kuramoto oscillators */
#define NIMCP_NATURAL_FREQUENCY_HZ             10.0f

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_NEURAL_CONSTANTS_H */
