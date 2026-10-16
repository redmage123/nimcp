//=============================================================================
// nimcp_brain_init_hippocampus.h - Hippocampus Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_hippocampus.h
 * @brief Hippocampus Initialization Functions
 *
 * WHAT: Initialization functions for hippocampus (memory and navigation)
 * WHY:  SRP refactoring - separate hippocampus initialization logic
 * HOW:  Creates hippocampus adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - CA1, CA3: Pyramidal cells for memory encoding/retrieval
 * - Dentate Gyrus: Pattern separation (sparse coding)
 * - Entorhinal Cortex: Grid cells, path integration
 * - Connections to neocortex, amygdala, prefrontal cortex
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of memory consolidation
 * - Thalamic Bridge: Anterior nucleus routing of memory signals
 * - Quantum Bridge: Grover-accelerated memory search
 *
 * @version Phase H1: Hippocampus Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_HIPPOCAMPUS_H
#define NIMCP_BRAIN_INIT_HIPPOCAMPUS_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Hippocampus Initialization API
//=============================================================================

/**
 * @brief Initialize hippocampus subsystem
 *
 * WHAT: Creates hippocampus adapter and connects all integration bridges
 * WHY:  Enable episodic memory and spatial navigation capabilities
 * HOW:  Creates adapter, then substrate/thalamic/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Hippocampus is essential for episodic memory formation
 * - Damage causes anterograde amnesia (inability to form new memories)
 * - Critical for spatial navigation (place cells, grid cells)
 *
 * INITIALIZATION ORDER:
 * 1. Create hippocampus adapter (place/grid cells, memory encoder)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create thalamic bridge (signal routing via anterior nucleus)
 * 4. Create quantum bridge (accelerated memory search)
 * 5. Connect to cortical areas for consolidation
 * 6. Connect to amygdala for emotional tagging
 * 7. Connect to training system for learning
 * 8. Connect to immune system for neuroinflammation effects
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_hippocampus_subsystem(brain_t brain);

/**
 * @brief Initialize hippocampus substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Memory consolidation depends on metabolic state (ATP, sleep)
 * HOW:  Links hippocampus to substrate for biologically-realistic memory
 *
 * EFFECTS:
 * - Low ATP -> Reduced memory encoding efficiency
 * - High fatigue -> Impaired consolidation
 * - Sleep state -> Enhanced replay and consolidation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_hippocampus_substrate_bridge(brain_t brain);

/**
 * @brief Initialize hippocampus thalamic bridge
 *
 * WHAT: Creates bridge to thalamic router for signal routing
 * WHY:  Memory signals require thalamic coordination with cortex
 * HOW:  Routes memory signals through anterior nucleus
 *
 * PATHWAYS:
 * - Episodic memory -> Anterior nucleus -> Prefrontal cortex
 * - Spatial navigation -> Anterior nucleus -> Parietal cortex
 * - Emotional memory -> Anterior nucleus -> Cingulate cortex
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_hippocampus_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize hippocampus quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for accelerated processing
 * WHY:  Memory search over large stores benefits from quantum speedup
 * HOW:  Uses Grover search for O(sqrt(N)) memory access
 *
 * APPLICATIONS:
 * - Memory retrieval: Find matching memory from partial cue
 * - Pattern completion: Complete pattern from degraded input
 * - Spatial navigation: Optimize path through quantum walk
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_hippocampus_quantum_bridge(brain_t brain);

/**
 * @brief Connect hippocampus to cortical areas
 *
 * WHAT: Links hippocampus to neocortical regions for consolidation
 * WHY:  Systems consolidation transfers memories to cortex
 * HOW:  Registers callback for memory transfer during consolidation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hippocampus_to_cortex(brain_t brain);

/**
 * @brief Connect hippocampus to amygdala
 *
 * WHAT: Links hippocampus to amygdala for emotional memory tagging
 * WHY:  Emotional significance enhances memory formation
 * HOW:  Bidirectional connection for emotional modulation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hippocampus_to_amygdala(brain_t brain);

/**
 * @brief Connect hippocampus to training system
 *
 * WHAT: Links hippocampus to training for memory-based learning
 * WHY:  Experience-dependent plasticity requires training integration
 * HOW:  Registers hippocampus adapter with training context
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hippocampus_to_training(brain_t brain);

/**
 * @brief Connect hippocampus to immune system
 *
 * WHAT: Links hippocampus to brain immune for inflammation modulation
 * WHY:  Neuroinflammation affects memory formation and consolidation
 * HOW:  Registers for cytokine signals that modulate plasticity
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hippocampus_to_immune(brain_t brain);

/**
 * @brief Connect hippocampus to sleep system
 *
 * WHAT: Links hippocampus to sleep/wake system for consolidation
 * WHY:  Sleep is critical for memory consolidation and replay
 * HOW:  Registers for sleep state changes to trigger replay
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hippocampus_to_sleep(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_HIPPOCAMPUS_H */
