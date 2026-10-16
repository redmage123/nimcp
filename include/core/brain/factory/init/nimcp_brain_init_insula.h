//=============================================================================
// nimcp_brain_init_insula.h - Insula Region Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_insula.h
 * @brief Insula Region Initialization Functions
 *
 * WHAT: Initialization functions for Insula region (interoception, emotion, social)
 * WHY:  SRP refactoring - separate Insula initialization logic
 * HOW:  Creates Insula adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Anterior insula: Interoceptive awareness, emotional salience
 * - Posterior insula: Somatosensory integration
 * - Mid-insula: Transition between visceral and cognitive processing
 * - Key connections: Limbic system, somatosensory cortex, prefrontal cortex
 *
 * FUNCTIONAL DOMAINS:
 * - Interoception: Body signal awareness (heartbeat, breathing, hunger)
 * - Emotional awareness: Subjective feeling states
 * - Disgust/social: Physical and moral disgust, empathy, trust
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: Metabolic modulation of body awareness
 * - Thalamic Bridge: Signal routing through ventral posterior nuclei
 * - Quantum Bridge: Superposition-based body signal integration
 *
 * @version Phase I1: Insula Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_INSULA_H
#define NIMCP_BRAIN_INIT_INSULA_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Insula Region Initialization API
//=============================================================================

/**
 * @brief Initialize Insula subsystem
 *
 * WHAT: Creates Insula adapter and connects all integration bridges
 * WHY:  Enable interoception, emotional awareness, and social emotion processing
 * HOW:  Creates adapter, then substrate/thalamic/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Insula is the hub for body-brain communication
 * - Damage causes loss of interoceptive awareness
 * - Critical for emotional experience and social cognition
 *
 * INITIALIZATION ORDER:
 * 1. Create Insula adapter (interoception/emotion/social)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create quantum bridge (parallel signal integration)
 * 4. Connect to limbic system
 * 5. Connect to somatosensory cortex
 * 6. Connect to emotional system
 * 7. Connect to immune system
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_insula_subsystem(brain_t brain);

/**
 * @brief Initialize Insula's quantum bridge
 *
 * WHAT: Creates bridge for quantum-accelerated body signal processing
 * WHY:  Multi-channel interoception benefits from parallel evaluation
 * HOW:  Uses quantum superposition for body state estimation
 *
 * APPLICATIONS:
 * - Interoceptive integration: Fuse multi-channel body signals
 * - Emotional evaluation: Superposition of emotion hypotheses
 * - Somatic marker search: Fast gut-feeling retrieval
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_insula_quantum_bridge(brain_t brain);

/**
 * @brief Connect Insula to limbic system
 *
 * WHAT: Links Insula to amygdala, hippocampus, and cingulate
 * WHY:  Emotional processing requires limbic integration
 * HOW:  Registers bidirectional callbacks for emotion signals
 *
 * CONNECTIONS:
 * - Amygdala: Fear and threat processing
 * - Hippocampus: Emotional memory context
 * - Anterior cingulate: Emotional conflict monitoring
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_insula_to_limbic(brain_t brain);

/**
 * @brief Connect Insula to somatosensory cortex
 *
 * WHAT: Links Insula to S1/S2 for body state awareness
 * WHY:  Interoception requires somatosensory input
 * HOW:  Registers body signal callbacks
 *
 * CONNECTIONS:
 * - S1: Primary somatosensory input
 * - S2: Secondary somatosensory integration
 * - Posterior parietal: Body schema
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_insula_to_somatosensory(brain_t brain);

/**
 * @brief Connect Insula to emotional system
 *
 * WHAT: Links Insula to emotional tagging and emotional state systems
 * WHY:  Insula is central to subjective emotional experience
 * HOW:  Registers emotion state callbacks for bidirectional communication
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_insula_to_emotional(brain_t brain);

/**
 * @brief Connect Insula to immune system
 *
 * WHAT: Links Insula to brain immune for inflammation awareness
 * WHY:  Immune signals affect interoceptive awareness (sickness behavior)
 * HOW:  Registers for cytokine signals
 *
 * EFFECTS:
 * - Inflammation → Fatigue and malaise
 * - Immune activation → Social withdrawal
 * - Cytokine signals → Body state changes
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_insula_to_immune(brain_t brain);

/**
 * @brief Connect Insula to theory of mind
 *
 * WHAT: Links Insula to ToM for empathy and social cognition
 * WHY:  Insula processes empathic resonance
 * HOW:  Registers callbacks for social signal exchange
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_insula_to_theory_of_mind(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_INSULA_H */
