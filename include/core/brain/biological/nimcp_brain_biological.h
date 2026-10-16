//=============================================================================
// nimcp_brain_biological.h - Biological Subsystems Module
//=============================================================================
/**
 * @file nimcp_brain_biological.h
 * @brief Extracted biological subsystems for brain initialization
 *
 * This module contains initialization functions for biological subsystems:
 * - Glial integration (astrocytes and glial support)
 * - Neuromodulator systems (dopamine, serotonin, acetylcholine, norepinephrine)
 * - Pink noise neuromodulation (1/f noise for exploration-exploitation)
 * - Multimodal integration (visual, audio, speech cortices)
 * - Spatial neuromodulation (volume transmission with quantum walk)
 *
 * EXTRACTED FROM: src/core/brain/nimcp_brain.c (lines 1347-1831)
 * PURPOSE: Reduce nimcp_brain.c complexity by separating biological systems
 *
 * @author NIMCP Development Team
 * @version 2.7.0
 * @date 2025-11-19
 */

#ifndef NIMCP_BRAIN_BIOLOGICAL_H
#define NIMCP_BRAIN_BIOLOGICAL_H

#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Function Declarations
//=============================================================================

/**
 * Initialize glial integration subsystem
 *
 * Sets up astrocytes and glial support structures for neuromodulation and
 * information processing. Integrates with the neural network.
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool init_glial_subsystem(brain_t brain);

/**
 * Initialize multimodal subsystems
 *
 * Sets up:
 * - Visual cortex (V1-like processing with orientation selectivity)
 * - Audio cortex (tonotopic A1-like processing)
 * - Speech cortex (Wernicke-like phoneme analysis)
 * - Multimodal integration layer (binds modalities)
 * - NLP network (language processing)
 * - Integrated feature buffer (unified representation)
 *
 * BIOLOGICAL MOTIVATION:
 * - Multi-sensory integration in human cortex
 * - Superior colliculus coordinates multiple modalities
 * - Superior temporal sulcus integrates visual-audio
 * - Wernicke's area processes language from multiple sources
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool init_multimodal_subsystems(brain_t brain);

/**
 * Initialize pink noise neuromodulation subsystem
 *
 * Enables 1/f noise-modulated neurotransmitters for exploration-exploitation
 * balance. Pink noise (power spectrum ∝ 1/f) provides correlations across
 * multiple timescales, enabling adaptive learning.
 *
 * BIOLOGICAL MOTIVATION:
 * - Dopamine neurons exhibit 1/f noise in firing patterns
 * - Serotonin fluctuations follow pink spectrum
 * - Multi-timescale correlations enable context-dependent learning
 * - Enables exploration-exploitation balance in reinforcement learning
 *
 * INTEGRATION:
 * - Modulates learning rates via dopamine
 * - Scales attention via acetylcholine
 * - Enables exploration via pink noise
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 *
 * @version 2.7.0 Phase 8.6
 */
bool init_pink_noise_subsystem(brain_t brain);

/**
 * Initialize full neuromodulator system
 *
 * Sets up the core neuromodulator system with personality-modulated baselines.
 * Enables mental health interventions to adjust neurotransmitter levels.
 *
 * BIOLOGICAL MOTIVATION:
 * - Neurotransmitters regulate mood, attention, arousal, and learning
 * - Mental health disorders often involve chemical imbalances
 * - Interventions can modulate levels to restore healthy functioning
 * - Personality traits correlate with baseline neurotransmitter levels
 *
 * PERSONALITY MAPPING:
 * - Dopamine: Extraversion (reward-seeking, social motivation)
 * - Serotonin: Inverse of Neuroticism (mood stability, impulse control)
 * - Acetylcholine: Openness (intellectual curiosity, learning)
 * - Norepinephrine: Conscientiousness (sustained alertness)
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 *
 * @version 2.7.0
 */
bool init_neuromodulator_system(brain_t brain);

/**
 * Initialize spatial neuromodulator system with quantum walk diffusion
 *
 * Sets up spatially-distributed neuromodulation with optional quantum
 * walk acceleration for faster diffusion through neural network.
 *
 * BIOLOGICAL MOTIVATION:
 * - Volume transmission: Neuromodulators diffuse through extracellular space
 * - Glial mediation: Astrocytes regulate neuromodulator concentrations
 * - Quantum walk: O(√N) speedup for diffusion on neural network graph
 *
 * INTEGRATION WITH BRAIN:
 * - Requires glial integration to be initialized first
 * - Uses quantum walk configuration from brain config
 * - Spatially modulates synaptic transmission based on local concentrations
 *
 * PERFORMANCE:
 * - Classical diffusion: O(N) for N neurons
 * - Quantum walk: O(√N) with potential speedup
 * - Decoherence rate controls quantum-classical mixing
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 *
 * @version 2.7.0 Phase C2.1 (Quantum Walk Integration)
 */
bool init_spatial_neuromod_system(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_BIOLOGICAL_H
