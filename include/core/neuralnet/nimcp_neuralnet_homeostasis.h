//=============================================================================
// nimcp_neuralnet_homeostasis.h - Homeostatic Plasticity
//=============================================================================
/**
 * @file nimcp_neuralnet_homeostasis.h
 * @brief Homeostatic plasticity mechanisms
 *
 * RESPONSIBILITY: Activity regulation and stability maintenance
 *
 * This module provides:
 * - Homeostatic plasticity (maintains target firing rate)
 * - Calcium dynamics for metaplasticity
 * - Adaptive threshold adjustment
 * - Network-wide homeostasis maintenance
 */

#ifndef NIMCP_NEURALNET_HOMEOSTASIS_H
#define NIMCP_NEURALNET_HOMEOSTASIS_H

#include "core/neuralnet/nimcp_neuralnet.h"
#include <stdbool.h>
#include <stdint.h>

// Forward declaration for immune system integration
typedef struct brain_immune_system brain_immune_system_t;

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Public API
//=============================================================================

/**
 * @brief Apply homeostatic plasticity to a neuron
 *
 * WHY: Maintains target firing rate, prevents runaway excitation/silence
 * COMPLEXITY: O(s) where s = num_synapses
 *
 * @param network Neural network
 * @param neuron_id Neuron to regulate
 * @param timestamp Current network time
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_apply_homeostasis(neural_network_t network, uint32_t neuron_id,
                                                   uint64_t timestamp);

/**
 * @brief Maintain homeostasis across entire network
 *
 * WHY: Periodic maintenance to prevent network instability
 * COMPLEXITY: O(n) where n = num_neurons
 *
 * @param network Neural network
 * @param timestamp Current network time
 */
NIMCP_EXPORT void neural_network_maintain_homeostasis(neural_network_t network, uint64_t timestamp);

/**
 * @brief Adapt neuron threshold based on activity level
 *
 * WHY: Implements adaptive threshold for spiking neurons based on current activity
 * COMPLEXITY: O(1)
 *
 * @param network Neural network
 * @param neuron_id Neuron to adapt
 * @param activity_level Current activity level
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_adapt_threshold_by_activity(neural_network_t network, uint32_t neuron_id,
                                                              float activity_level);

/**
 * @brief Perform periodic network maintenance
 *
 * WHY: Weight normalization, cleanup, stability checks
 * COMPLEXITY: O(n * s) where n = neurons, s = synapses
 *
 * @param network Neural network
 * @param timestamp Current network time
 */
NIMCP_EXPORT void neural_network_maintain(neural_network_t network, uint64_t timestamp);

//=============================================================================
// Immune System Integration API
//=============================================================================

/**
 * @brief Apply immune-mediated inflammation effects to homeostasis
 *
 * WHAT: Modulate homeostatic set points based on immune inflammation
 * WHY: Inflammation alters neural homeostasis (fever analogy - changes baseline)
 * HOW: Inflammation increases target activity and metabolic demand
 *
 * BIOLOGICAL BASIS:
 * - Pro-inflammatory cytokines (IL-1, IL-6, TNF-α) increase neural excitability
 * - Inflammation shifts homeostatic set points upward (fever-like state)
 * - Chronic inflammation causes allostatic load on neural health
 *
 * @param network Neural network
 * @param inflammation_level Inflammation severity (0.0-1.0, 0=none, 1=storm)
 * @param region_id Affected region (0 = global)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_apply_immune_inflammation(
    neural_network_t network,
    float inflammation_level,
    uint32_t region_id
);

/**
 * @brief Apply anti-inflammatory cytokine effects (IL-10) to aid recovery
 *
 * WHAT: Restore homeostatic set points toward baseline after threat resolution
 * WHY: Anti-inflammatory signals help return neural activity to normal
 * HOW: IL-10 reduces target activity and metabolic demand
 *
 * BIOLOGICAL BASIS:
 * - IL-10 is the primary anti-inflammatory cytokine
 * - Reduces pro-inflammatory cytokine production
 * - Aids resolution phase and return to baseline homeostasis
 *
 * @param network Neural network
 * @param il10_concentration IL-10 concentration (0.0-1.0)
 * @param region_id Affected region (0 = global)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_apply_anti_inflammatory(
    neural_network_t network,
    float il10_concentration,
    uint32_t region_id
);

/**
 * @brief Modulate synaptic scaling rates based on cytokine levels
 *
 * WHAT: Adjust homeostatic plasticity speed based on immune state
 * WHY: Cytokines modulate learning and plasticity rates
 * HOW: Pro-inflammatory cytokines slow scaling, anti-inflammatory restore it
 *
 * BIOLOGICAL BASIS:
 * - TNF-α modulates synaptic scaling and AMPA receptor trafficking
 * - IL-1β affects LTP/LTD induction
 * - Immune activation temporarily alters plasticity mechanisms
 *
 * @param network Neural network
 * @param neuron_id Neuron to modulate
 * @param cytokine_modulation Modulation factor (-1.0 to 1.0, negative slows, positive speeds)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_modulate_scaling_rate(
    neural_network_t network,
    uint32_t neuron_id,
    float cytokine_modulation
);

/**
 * @brief Increase metabolic demand due to immune activation
 *
 * WHAT: Temporarily increase energy requirements during immune response
 * WHY: Immune activation is energetically expensive, reduces available resources
 * HOW: Increase homeostatic pressure, reduce plasticity temporarily
 *
 * BIOLOGICAL BASIS:
 * - Immune response requires significant ATP
 * - Competes with neural activity for metabolic resources
 * - Temporary reduction in synaptic plasticity during acute response
 *
 * @param network Neural network
 * @param metabolic_load Metabolic load increase (0.0-1.0)
 * @param region_id Affected region (0 = global)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_apply_immune_metabolic_load(
    neural_network_t network,
    float metabolic_load,
    uint32_t region_id
);

/**
 * @brief Accumulate allostatic load from chronic inflammation
 *
 * WHAT: Long-term health cost from sustained immune activation
 * WHY: Chronic inflammation damages neural health and homeostatic capacity
 * HOW: Accumulate allostatic load metric, reduce homeostatic effectiveness
 *
 * BIOLOGICAL BASIS:
 * - Chronic inflammation causes neurodegeneration
 * - Sustained cytokine elevation impairs homeostatic mechanisms
 * - Allostatic load accumulates over time, reducing resilience
 *
 * @param network Neural network
 * @param neuron_id Neuron accumulating load
 * @param inflammation_duration Duration of inflammation (ms)
 * @param inflammation_level Average inflammation level (0.0-1.0)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_accumulate_allostatic_load(
    neural_network_t network,
    uint32_t neuron_id,
    uint64_t inflammation_duration,
    float inflammation_level
);

/**
 * @brief Compute homeostatic health metric including immune state
 *
 * WHAT: Calculate overall homeostatic health considering immune factors
 * WHY: Integrate immune state into health monitoring
 * HOW: Combine activity balance, plasticity capacity, and allostatic load
 *
 * BIOLOGICAL BASIS:
 * - Homeostatic health depends on both neural and immune state
 * - Inflammation, metabolic load, and allostatic burden reduce health
 * - Healthy homeostasis requires balanced immune and neural function
 *
 * @param network Neural network
 * @param neuron_id Neuron to assess
 * @return Health metric (0.0-1.0, 1.0 = optimal health)
 */
NIMCP_EXPORT float neural_network_compute_homeostatic_health(
    neural_network_t network,
    uint32_t neuron_id
);

/**
 * @brief Connect immune system to homeostasis module
 *
 * WHAT: Link immune system for bidirectional communication
 * WHY: Enable immune state to influence homeostasis
 * HOW: Store immune system pointer in network context
 *
 * @param network Neural network
 * @param immune_system Brain immune system
 * @return true on success, false on error
 */
NIMCP_EXPORT bool neural_network_connect_immune_system(
    neural_network_t network,
    brain_immune_system_t* immune_system
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURALNET_HOMEOSTASIS_H
