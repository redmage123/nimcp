//=============================================================================
// nimcp_brain_complex_oscillations.h - Complex Phasor Oscillation Tracking
//=============================================================================
/**
 * @file nimcp_brain_complex_oscillations.h
 * @brief Complex oscillation support for neural phase and amplitude tracking
 *
 * WHAT: Phasor-based oscillation tracking with phase coherence and PAC
 * WHY:  Brain uses phase relationships for binding, memory, spatial coding
 * HOW:  Track neural_phasor_t per neuron, compute coherence and PAC
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Phase Coding in Neural Oscillations:
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  Neural oscillations encode information in PHASE:           │
 *   │  - Hippocampal place cells: theta phase = position          │
 *   │  - Working memory: gamma phase = item order                 │
 *   │  - Sensory binding: phase synchrony across regions          │
 *   │                                                              │
 *   │  Complex phasor representation:                             │
 *   │    z(t) = A(t)·e^(iφ(t))                                    │
 *   │    - A(t): Instantaneous amplitude (strength)               │
 *   │    - φ(t): Instantaneous phase (timing)                     │
 *   │                                                              │
 *   │  Advantages over amplitude-only:                            │
 *   │  - Phase coherence: measure synchrony across neurons        │
 *   │  - PAC detection: theta phase modulates gamma amplitude     │
 *   │  - Phase differences: compute relative timing               │
 *   └──────────────────────────────────────────────────────────────┘
 *
 * INTEGRATION WITH EXISTING SYSTEM:
 * - Builds on nimcp_brain_oscillations.h (frequency band power)
 * - Uses nimcp_complex_math.h (phasor operations)
 * - Opt-in via brain_config_t.complex_oscillation_enabled
 * - Backward compatible (disabled by default)
 *
 * PERFORMANCE:
 * - Phasor update per neuron: ~10ns (2x float assignment + phase calc)
 * - Coherence across N neurons: ~0.8µs for N=1000
 * - PAC detection: ~2µs for N=1000 samples
 * - Memory overhead: 8 bytes/neuron (2x float for phasor)
 *
 * USE CASES:
 * - Track instantaneous phase and amplitude per neuron
 * - Measure phase coherence across neuron populations
 * - Detect phase-amplitude coupling (PAC) for binding
 * - Analyze cross-frequency phase relationships
 *
 * @author NIMCP Development Team
 * @date 2025-11-22
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_COMPLEX_OSCILLATIONS_H
#define NIMCP_BRAIN_COMPLEX_OSCILLATIONS_H

#include "core/brain/nimcp_brain.h"
#include "utils/math/nimcp_complex_math.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Complex oscillation state tracker
 *
 * WHAT: Tracks instantaneous phase and amplitude for each neuron
 * WHY:  Enable phasor-based analysis beyond frequency band power
 * HOW:  Store neural_phasor_t array, update from neural activity
 */
typedef struct brain_complex_oscillation_state_struct {
    neural_phasor_t* neuron_phasors;  /**< Phasor per neuron [num_neurons] */
    uint32_t num_neurons;              /**< Number of neurons tracked */

    // Cached metrics (computed on demand)
    float last_coherence;              /**< Last computed coherence [0-1] */
    float last_mean_phase;             /**< Last mean phase [-π, π] */
    float last_phase_variance;         /**< Last phase variance [0-1] */
    bool metrics_valid;                /**< Are cached metrics valid? */

    // Configuration
    float phase_update_rate;           /**< Phase update rate (radians/step) */
    float amplitude_decay;             /**< Amplitude decay per step [0-1] */
} brain_complex_oscillation_state_t;

/**
 * @brief Phase coherence results
 */
typedef struct {
    float coherence;                   /**< Overall phase coherence [0-1] */
    float mean_phase;                  /**< Circular mean phase [-π, π] */
    float phase_variance;              /**< Phase variance [0-1] */
    float mean_amplitude;              /**< Mean amplitude across neurons */
    uint32_t num_neurons;              /**< Number of neurons analyzed */
} phase_coherence_result_t;

/**
 * @brief Phase-amplitude coupling (PAC) results
 */
typedef struct {
    float modulation_index;            /**< PAC strength [0-1] */
    float preferred_phase;             /**< Preferred phase for high amplitude [-π, π] */
    float phase_bin_count;             /**< Number of phase bins used (18 = 20° bins) */
    float significance;                /**< Statistical significance [0-1] */
} pac_result_t;

//=============================================================================
// Lifecycle Management
//=============================================================================

/**
 * @brief Create complex oscillation state
 *
 * WHAT: Allocate and initialize phasor tracking for brain
 * WHY:  Enable complex oscillation analysis
 * HOW:  Allocate neural_phasor_t array, initialize to zero
 *
 * @param num_neurons Number of neurons to track
 * @param phase_update_rate Phase increment per step (default: 0.1 rad)
 * @param amplitude_decay Amplitude decay factor (default: 0.95)
 * @return Complex oscillation state or NULL on failure
 *
 * COMPLEXITY: O(N) where N = num_neurons
 * MEMORY: 8 * num_neurons bytes + struct overhead
 */
brain_complex_oscillation_state_t* brain_complex_oscillation_create(
    uint32_t num_neurons,
    float phase_update_rate,
    float amplitude_decay
);

/**
 * @brief Destroy complex oscillation state
 *
 * @param state State to destroy
 */
void brain_complex_oscillation_destroy(
    brain_complex_oscillation_state_t* state
);

//=============================================================================
// Phasor Tracking
//=============================================================================

/**
 * @brief Update phasors from neural activity
 *
 * WHAT: Convert neural activations to phasor representation
 * WHY:  Track instantaneous amplitude and phase per neuron
 * HOW:  Amplitude from activation, phase increments at update rate
 *
 * @param state Complex oscillation state
 * @param activations Neural activation values [num_neurons]
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(N) where N = num_neurons
 *
 * ALGORITHM:
 * For each neuron i:
 *   amplitude[i] = activations[i] * amplitude_decay
 *   phase[i] += phase_update_rate
 *   phasor[i] = amplitude[i] * e^(i*phase[i])
 */
bool brain_complex_oscillation_update(
    brain_complex_oscillation_state_t* state,
    const float* activations
);

/**
 * @brief Get phasor for specific neuron
 *
 * @param state Complex oscillation state
 * @param neuron_index Neuron index
 * @param phasor Output phasor
 * @return true on success, false on invalid index
 */
bool brain_complex_oscillation_get_phasor(
    const brain_complex_oscillation_state_t* state,
    uint32_t neuron_index,
    neural_phasor_t* phasor
);

/**
 * @brief Set phasor for specific neuron
 *
 * @param state Complex oscillation state
 * @param neuron_index Neuron index
 * @param phasor New phasor value
 * @return true on success, false on invalid index
 */
bool brain_complex_oscillation_set_phasor(
    brain_complex_oscillation_state_t* state,
    uint32_t neuron_index,
    neural_phasor_t phasor
);

//=============================================================================
// Phase Coherence Analysis
//=============================================================================

/**
 * @brief Compute phase coherence across all neurons
 *
 * WHAT: Measure phase synchrony using inter-trial phase coherence (ITPC)
 * WHY:  High coherence → synchronized oscillations, binding
 * HOW:  ITPC = |mean(phasor[i]/|phasor[i]|)|
 *
 * @param state Complex oscillation state
 * @param result Output coherence results
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(N) where N = num_neurons
 *
 * COHERENCE VALUES:
 * - 0.0-0.2: Desynchronized (independent phases)
 * - 0.2-0.5: Partially synchronized
 * - 0.5-0.8: Highly synchronized
 * - 0.8-1.0: Perfect phase locking
 */
bool brain_complex_oscillation_compute_coherence(
    brain_complex_oscillation_state_t* state,
    phase_coherence_result_t* result
);

/**
 * @brief Compute phase coherence for neuron subset
 *
 * @param state Complex oscillation state
 * @param neuron_indices Indices of neurons to analyze
 * @param num_neurons Number of neurons in subset
 * @param result Output coherence results
 * @return true on success, false on failure
 *
 * USE CASE: Analyze coherence within specific brain regions
 */
bool brain_complex_oscillation_compute_coherence_subset(
    brain_complex_oscillation_state_t* state,
    const uint32_t* neuron_indices,
    uint32_t num_neurons,
    phase_coherence_result_t* result
);

/**
 * @brief Compute pairwise phase synchrony
 *
 * WHAT: Measure phase locking between two neuron populations
 * WHY:  Assess inter-regional synchronization
 * HOW:  PLV = |mean(exp(i*(phase1 - phase2)))|
 *
 * @param state Complex oscillation state
 * @param indices1 First population neuron indices
 * @param num1 Number of neurons in first population
 * @param indices2 Second population neuron indices
 * @param num2 Number of neurons in second population
 * @return Synchrony [0-1], or -1.0 on error
 *
 * COMPLEXITY: O(min(N1, N2))
 */
float brain_complex_oscillation_compute_synchrony(
    brain_complex_oscillation_state_t* state,
    const uint32_t* indices1,
    uint32_t num1,
    const uint32_t* indices2,
    uint32_t num2
);

//=============================================================================
// Phase-Amplitude Coupling (PAC)
//=============================================================================

/**
 * @brief Detect phase-amplitude coupling
 *
 * WHAT: Measure how low-freq phase modulates high-freq amplitude
 * WHY:  PAC indicates cross-frequency binding (theta-gamma coupling)
 * HOW:  Use phasor_pac_modulation_index() from complex_math
 *
 * @param state Complex oscillation state
 * @param phase_indices Low-frequency phase neuron indices
 * @param num_phase Number of phase neurons
 * @param amplitude_values High-frequency amplitude values
 * @param num_amplitude Number of amplitude samples (must match num_phase)
 * @param result Output PAC results
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(N) where N = num_phase
 *
 * NEUROSCIENCE:
 * - Theta-gamma PAC: Memory encoding/retrieval (hippocampus)
 * - Alpha-beta PAC: Attention gating (cortex)
 * - Delta-gamma PAC: Sleep spindles
 *
 * PAC VALUES:
 * - 0.0-0.2: Weak/no coupling
 * - 0.2-0.4: Moderate coupling
 * - 0.4-1.0: Strong coupling (significant binding)
 */
bool brain_complex_oscillation_compute_pac(
    brain_complex_oscillation_state_t* state,
    const uint32_t* phase_indices,
    uint32_t num_phase,
    const float* amplitude_values,
    uint32_t num_amplitude,
    pac_result_t* result
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Reset all phasors to zero
 *
 * @param state Complex oscillation state
 */
void brain_complex_oscillation_reset(
    brain_complex_oscillation_state_t* state
);

/**
 * @brief Invalidate cached metrics
 *
 * @param state Complex oscillation state
 *
 * NOTE: Called automatically after update operations
 */
void brain_complex_oscillation_invalidate_cache(
    brain_complex_oscillation_state_t* state
);

/**
 * @brief Get number of neurons tracked
 *
 * @param state Complex oscillation state
 * @return Number of neurons
 */
uint32_t brain_complex_oscillation_get_num_neurons(
    const brain_complex_oscillation_state_t* state
);

/**
 * @brief Check if complex oscillations are enabled for brain
 *
 * @param brain Brain handle
 * @return true if complex oscillations enabled, false otherwise
 */
bool brain_complex_oscillation_is_enabled(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_COMPLEX_OSCILLATIONS_H */
