//=============================================================================
// nimcp_brain_shannon.h - Shannon Information Theory API for Brain
//=============================================================================
/**
 * @file nimcp_brain_shannon.h
 * @brief Shannon information theory functions for brain information flow monitoring
 *
 * This module provides APIs for:
 * - Shannon information flow monitoring (Phase C4)
 * - Quantum-Shannon accelerated diffusion (Phase C4.1)
 * - Cross-modal information flow tracking (Phase C4.7)
 *
 * EXTRACTED FROM: nimcp_brain.c (lines 7339-7739)
 * DATE: 2025-11-19
 */

#ifndef NIMCP_BRAIN_SHANNON_H
#define NIMCP_BRAIN_SHANNON_H

#include "core/brain/nimcp_brain.h"
#include "information/nimcp_shannon.h"
#include "utils/quantum/nimcp_quantum_shannon.h"
#include "information/nimcp_cross_modal.h"
#include "utils/platform/nimcp_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Phase C4: Shannon Information Theory API
//=============================================================================

/**
 * @brief Enable Shannon information flow monitoring
 *
 * WHAT: Activate real-time Shannon metrics during learning/inference
 * WHY:  Monitor channel capacity, detect bottlenecks, optimize information flow
 * HOW:  Sets enable_shannon_monitoring flag in brain
 *
 * PERFORMANCE IMPACT: ~5-10% overhead during learning/inference
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 */
NIMCP_EXPORT void brain_enable_shannon_monitoring(brain_t brain, bool enable);

/**
 * @brief Get last Shannon network metrics
 *
 * WHAT: Retrieve most recent Shannon analysis results
 * WHY:  Allow external monitoring of information flow characteristics
 * HOW:  Returns copy of last_shannon_metrics from brain
 *
 * @param brain Brain handle
 * @param metrics Output metrics structure
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_get_shannon_metrics(brain_t brain, shannon_network_metrics_t* metrics);

/**
 * @brief Set custom Shannon configuration
 *
 * WHAT: Override default Shannon analysis parameters
 * WHY:  Tune accuracy vs performance tradeoff
 * HOW:  Updates brain->shannon_config
 *
 * @param brain Brain handle
 * @param config Custom Shannon configuration
 */
NIMCP_EXPORT void brain_set_shannon_config(brain_t brain, const shannon_config_t* config);

//=============================================================================
// Phase C4.1: Quantum-Shannon Diffusion API
//=============================================================================

/**
 * @brief Enable quantum-Shannon accelerated diffusion
 *
 * WHAT: Activate √N speedup quantum walk diffusion with Shannon monitoring
 * WHY:  Quadratic speedup for neuromodulator propagation and real-time bottleneck detection
 * HOW:  Creates quantum_shannon_diffusion_t on brain network, enables in learning/inference
 *
 * PERFORMANCE IMPACT: 2-50x speedup (topology dependent), 3× memory overhead
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 * @param source_neuron_id Initial source neuron for diffusion (0 = auto-select middle neuron)
 * @param source_information_bits Initial information content (default: 10.0 bits)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_enable_quantum_shannon_diffusion(brain_t brain, bool enable, uint32_t source_neuron_id, float source_information_bits);

/**
 * @brief Set quantum-Shannon mixing ratio
 *
 * WHAT: Control quantum vs classical diffusion blend
 * WHY:  Tune performance vs accuracy tradeoff
 * HOW:  Sets mixing_ratio [0=pure quantum, 1=pure classical]
 *
 * @param brain Brain handle
 * @param mixing_ratio Mix ratio [0.0-1.0]
 */
NIMCP_EXPORT void brain_set_quantum_shannon_mixing(brain_t brain, float mixing_ratio);

/**
 * @brief Set quantum-Shannon evolution steps
 *
 * WHAT: Control how many quantum steps per diffusion update
 * WHY:  More steps = better spreading, but slower
 * HOW:  Sets evolution_steps parameter
 *
 * @param brain Brain handle
 * @param steps Number of steps (10-1000, default: 100)
 */
NIMCP_EXPORT void brain_set_quantum_shannon_steps(brain_t brain, uint32_t steps);

/**
 * @brief Get last quantum-Shannon diffusion metrics
 *
 * WHAT: Retrieve most recent Shannon metrics from quantum diffusion
 * WHY:  Monitor speedup, bottlenecks, and information flow
 * HOW:  Returns last_quantum_shannon_metrics from brain
 *
 * @param brain Brain handle
 * @param metrics Output metrics structure
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_get_quantum_shannon_metrics(brain_t brain, shannon_diffusion_metrics_t* metrics);

/**
 * @brief Evolve quantum-Shannon diffusion manually
 *
 * WHAT: Manually trigger quantum-Shannon evolution
 * WHY:  For fine-grained control or testing
 * HOW:  Calls quantum_shannon_evolve() with configured steps
 *
 * @param brain Brain handle
 * @param num_steps Number of evolution steps (0 = use configured value)
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_evolve_quantum_shannon(brain_t brain, uint32_t num_steps);

//=============================================================================
// Phase C4.7: Cross-Modal Information Flow API
//=============================================================================

/**
 * @brief Enable cross-modal information flow monitoring
 *
 * WHAT: Activate real-time tracking of information flow between sensory modalities
 * WHY:  Monitor multi-sensory integration, detect bottlenecks, optimize routing
 * HOW:  Sets enable_cross_modal_monitoring flag and creates routing graph
 *
 * BIOLOGICAL BASIS: Superior temporal sulcus (audiovisual), superior colliculus (multisensory)
 * PERFORMANCE IMPACT: ~2-5% overhead during multimodal processing
 *
 * @param brain Brain handle
 * @param enable true to enable, false to disable
 */
NIMCP_EXPORT void brain_enable_cross_modal_monitoring(brain_t brain, bool enable);

/**
 * @brief Get cross-modal routing graph
 *
 * WHAT: Retrieve current cross-modal information routing graph
 * WHY:  Allow external analysis of multi-sensory integration pathways
 * HOW:  Returns pointer to brain's cross_modal_graph
 *
 * NOTE: Graph may be NULL if cross-modal monitoring not enabled
 *
 * @param brain Brain handle
 * @return Cross-modal routing graph (NULL if not enabled)
 */
NIMCP_EXPORT cross_modal_routing_graph_t* brain_get_cross_modal_graph(brain_t brain);

/**
 * @brief Get last multi-modal integration metrics
 *
 * WHAT: Retrieve most recent cross-modal integration metrics
 * WHY:  Monitor synergy, redundancy, and integration efficiency
 * HOW:  Returns copy of last_cross_modal_metrics from brain
 *
 * @param brain Brain handle
 * @param metrics Output metrics structure
 * @return true on success, false on error
 */
NIMCP_EXPORT bool brain_get_cross_modal_metrics(brain_t brain, multi_modal_integration_t* metrics);

/**
 * @brief Set cross-modal bottleneck detection threshold
 *
 * WHAT: Configure threshold for identifying cross-modal bottlenecks
 * WHY:  Tune sensitivity of bottleneck detection
 * HOW:  Sets cross_modal_bottleneck_threshold in brain
 *
 * @param brain Brain handle
 * @param threshold Efficiency threshold [0.0-1.0] (default: 0.5)
 */
NIMCP_EXPORT void brain_set_cross_modal_threshold(brain_t brain, float threshold);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_SHANNON_H
