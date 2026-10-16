//=============================================================================
// nimcp_brain_init_broca.h - Broca's Region Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_broca.h
 * @brief Broca's Region Initialization Functions
 *
 * WHAT: Initialization functions for Broca's region (language production)
 * WHY:  SRP refactoring - separate Broca initialization logic
 * HOW:  Creates Broca adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Brodmann areas 44 (pars opercularis) and 45 (pars triangularis)
 * - Syntax processing, phonological planning, speech motor control
 * - Connections to Wernicke's area, motor cortex, prefrontal cortex
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of speech fluency
 * - Thalamic Bridge: VA/VL routing of motor speech commands
 * - Quantum Bridge: Grover-accelerated lexical search
 *
 * @version Phase B3: Broca Full Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_BROCA_H
#define NIMCP_BRAIN_INIT_BROCA_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Broca's Region Initialization API
//=============================================================================

/**
 * @brief Initialize Broca's region subsystem
 *
 * WHAT: Creates Broca adapter and connects all integration bridges
 * WHY:  Enable language production capabilities in the brain
 * HOW:  Creates adapter, then substrate/thalamic/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Broca's area is essential for speech production
 * - Damage causes Broca's aphasia (non-fluent speech)
 * - Integrates syntax, phonology, and motor planning
 *
 * INITIALIZATION ORDER:
 * 1. Create Broca adapter (core syntax/phonological/motor)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create thalamic bridge (signal routing)
 * 4. Create quantum bridge (accelerated search)
 * 5. Connect to working memory
 * 6. Connect to training system
 * 7. Connect to immune system
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_broca_subsystem(brain_t brain);

/**
 * @brief Destroy the Broca's-area subsystem
 *
 * WHAT: Tear down Broca + speech-repair + phonological-loop +
 *       pragmatics + substrate/thalamic/quantum bridges. Idempotent;
 *       guards on NULL fields and clears each to NULL on exit.
 * WHY:  Without this called from brain_destroy, all of these subsystems
 *       leak on every brain teardown (S5-C1 / S5-C2 critical fix —
 *       phonological_loop in particular owns a malloced trace buffer +
 *       once-init mutex).
 * HOW:  Reverse-order tear-down of the init call sequence.
 *
 * @param brain Brain instance
 */
void nimcp_brain_factory_destroy_broca_subsystem(brain_t brain);

/**
 * @brief Initialize Broca's substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Speech fluency depends on metabolic state (ATP, fatigue)
 * HOW:  Links Broca to substrate for biologically-realistic production
 *
 * EFFECTS:
 * - Low ATP → Reduced word retrieval speed
 * - High fatigue → Simplified syntax, articulation errors
 * - Stress → Phonological planning disruption
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_broca_substrate_bridge(brain_t brain);

/**
 * @brief Initialize Broca's thalamic bridge
 *
 * WHAT: Creates bridge to thalamic router for signal routing
 * WHY:  Motor speech requires thalamic coordination with motor cortex
 * HOW:  Routes speech commands through VA/VL nuclei
 *
 * PATHWAYS:
 * - Motor commands → VA/VL → Motor cortex (M1)
 * - Syntactic structures → Pulvinar → Parietal integration
 * - Lexical requests → MD → Prefrontal cortex
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_broca_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize Broca's quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for accelerated processing
 * WHY:  Lexical search in large vocabularies benefits from quantum speedup
 * HOW:  Uses Grover search for O(sqrt(N)) lexical access
 *
 * APPLICATIONS:
 * - Lexical selection: Find best word from semantic constraints
 * - Syntax optimization: Choose most fluent syntactic arrangement
 * - Phoneme sequencing: Optimize articulatory trajectory
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_broca_quantum_bridge(brain_t brain);

/**
 * @brief Connect Broca's region to working memory
 *
 * WHAT: Links Broca to working memory for lexical access
 * WHY:  Speech production requires active maintenance of word candidates
 * HOW:  Registers Broca as WM consumer for language production slots
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_broca_to_working_memory(brain_t brain);

/**
 * @brief Connect Broca's region to training system
 *
 * WHAT: Links Broca to training for language production learning
 * WHY:  Speech production improves through practice and feedback
 * HOW:  Registers Broca adapter with training context
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_broca_to_training(brain_t brain);

/**
 * @brief Connect Broca's region to immune system
 *
 * WHAT: Links Broca to brain immune for inflammation modulation
 * WHY:  Neuroinflammation affects language production
 * HOW:  Registers for cytokine signals that modulate fluency
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_broca_to_immune(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_BROCA_H */
