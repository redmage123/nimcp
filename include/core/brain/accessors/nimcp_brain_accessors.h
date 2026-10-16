//=============================================================================
// nimcp_brain_accessors.h - Brain Subsystem Accessor Functions
//=============================================================================
/**
 * @file nimcp_brain_accessors.h
 * @brief Accessor functions for brain subsystems
 *
 * WHAT: Provides getter functions for all major brain subsystems
 * WHY:  Enables external modules to access brain components safely
 * HOW:  NULL-safe accessors with validation
 *
 * EXTRACTED FROM: nimcp_brain.c (lines 5097-5447)
 * DATE: 2025-11-19
 */

#ifndef NIMCP_BRAIN_ACCESSORS_H
#define NIMCP_BRAIN_ACCESSORS_H

#include "core/brain/nimcp_brain.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Simple Subsystem Accessors
//=============================================================================

/**
 * @brief Get glial integration subsystem
 * @param brain Brain handle
 * @return Glial integration handle or NULL if not available
 */
glial_integration_t* brain_get_glial(brain_t brain);

/**
 * @brief Get brain oscillations analyzer
 * @param brain Brain handle
 * @return Oscillation analyzer handle or NULL if not available
 */
brain_oscillation_analyzer_t* brain_get_oscillations(brain_t brain);

/**
 * @brief Get introspection context
 * @param brain Brain handle
 * @return Introspection context or NULL if not available
 */
introspection_context_t brain_get_introspection(brain_t brain);

/**
 * @brief Get ethics engine
 * @param brain Brain handle
 * @return Ethics engine handle or NULL if not available
 */
ethics_engine_t brain_get_ethics(brain_t brain);

/**
 * @brief Get salience evaluator
 * @param brain Brain handle
 * @return Salience evaluator handle or NULL if not available
 */
salience_evaluator_t brain_get_salience(brain_t brain);

/**
 * @brief Get consolidation handle
 * @param brain Brain handle
 * @return Consolidation handle or NULL if not available
 */
consolidation_handle_t brain_get_consolidation(brain_t brain);

/**
 * @brief Get curiosity engine
 * @param brain Brain handle
 * @return Curiosity engine handle or NULL if not available
 */
curiosity_engine_t brain_get_curiosity(brain_t brain);

/**
 * @brief Get knowledge system
 * @param brain Brain handle
 * @return Knowledge system handle or NULL if not available
 */
knowledge_system_t brain_get_knowledge(brain_t brain);

/**
 * @brief Get neural logic network
 * @param brain Brain handle
 * @return Logic network handle or NULL if not available
 */
neural_logic_network_t brain_get_logic(brain_t brain);

/**
 * @brief Get symbolic logic system
 * @param brain Brain handle
 * @return Symbolic logic handle or NULL if not available
 */
symbolic_logic_t* brain_get_symbolic_logic(brain_t brain);

/**
 * @brief Get pink noise generator
 * @param brain Brain handle
 * @return Pink noise handle (as void*) or NULL if not available
 */
void* brain_get_pink_noise(brain_t brain);

//=============================================================================
// Mirror Neuron & Empathy Functions
//=============================================================================

/**
 * @brief Get mirror neuron activations for Theory of Mind integration
 *
 * WHAT: Extract current mirror neuron activation pattern
 * WHY:  Enable ToM to infer agent intentions from observed actions
 * HOW:  Query mirror neuron system for all current activations
 *
 * BIOLOGICAL RATIONALE:
 * Mirror neurons in premotor cortex (F5) and inferior parietal lobule (IPL)
 * fire both during action execution and observation (Rizzolatti & Craighero, 2004).
 * This activation pattern enables Theory of Mind to infer "why" an agent is
 * performing an action, supporting empathy and social cognition.
 *
 * COMPLEXITY: O(n) where n = number of learned actions
 *
 * @param brain Brain handle
 * @param activations Output buffer (must be allocated by caller)
 * @param max_size Buffer size (number of floats)
 * @param out_size Actual number of activations returned
 * @return true on success, false on error
 */
bool brain_get_mirror_activations(brain_t brain, float* activations,
                                                uint32_t max_size, uint32_t* out_size);

/**
 * @brief Compute empathy response from mirror neuron activations
 *
 * WHAT: Generate empathetic emotional response from observed behavior
 * WHY:  Mirror neurons enable emotional contagion and empathy
 * HOW:  Observe action → activate mirror neurons → infer emotion → empathy
 *
 * BIOLOGICAL RATIONALE:
 * Empathy emerges from shared representations in mirror neuron system
 * (Preston & de Waal, 2002). When observing others' emotional expressions:
 * 1. Mirror neurons activate (action observation)
 * 2. Emotional system infers observed emotion
 * 3. Empathetic response generated (emotional contagion)
 *
 * This models the perception-action model of empathy.
 *
 * COMPLEXITY: O(n) where n = num_features
 *
 * @param brain Brain handle
 * @param observed_features Observed behavior features
 * @param num_features Number of features
 * @param empathy_valence Output: valence (-1 to 1)
 * @param empathy_arousal Output: arousal (0 to 1)
 * @param empathy_confidence Output: confidence (0 to 1)
 * @return true on success
 */
bool brain_compute_empathy(brain_t brain, const float* observed_features,
                                        uint32_t num_features, float* empathy_valence,
                                        float* empathy_arousal, float* empathy_confidence);

//=============================================================================
// Astrocyte High-Level API
//=============================================================================

/**
 * @brief Enable astrocyte network with automatic configuration
 *
 * WHAT: Initialize and integrate astrocyte network into brain
 * WHY:  Enable glial modulation of synaptic plasticity
 * HOW:  Create network → assign to synapses → enable modulation
 *
 * @param brain Brain handle
 * @param num_astrocytes Number of astrocytes (0 = auto-calculate from brain size)
 * @param coverage_radius_um Coverage radius in micrometers (0 = use default 75µm)
 * @return true on success, false on error
 */
bool brain_enable_astrocytes(brain_t brain, uint32_t num_astrocytes,
                                          float coverage_radius_um);

/**
 * @brief Get astrocyte network statistics
 *
 * @param brain Brain handle
 * @param stats Output statistics structure
 * @return true on success, false on error
 */
bool brain_get_astrocyte_stats(brain_t brain, astrocyte_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_ACCESSORS_H
