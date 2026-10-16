/**
 * @file nimcp_brain_init_cingulate.h
 * @brief Cingulate Cortex Initialization Functions
 *
 * WHAT: Initialization functions for Cingulate Cortex subsystem
 * WHY:  SRP refactoring - separate cingulate initialization logic
 * HOW:  Creates cingulate adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Anterior Cingulate Cortex (ACC): Brodmann areas 24, 32, 33
 *   - Conflict monitoring and error detection
 *   - Cognitive control signaling
 *   - Emotion-cognition integration
 * - Posterior Cingulate Cortex (PCC): Brodmann areas 23, 31
 *   - Self-referential processing
 *   - Default Mode Network hub
 *   - Autobiographical memory access
 *
 * INTEGRATION BRIDGES:
 * - Quantum Bridge: Grover-accelerated conflict resolution
 * - FEP Bridge: Prediction error modulation
 * - Executive Bridge: Top-down cognitive control
 *
 * @version Phase B4: Cingulate Cortex Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_CINGULATE_H
#define NIMCP_BRAIN_INIT_CINGULATE_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CINGULATE CORTEX INITIALIZATION API
 *===========================================================================*/

/**
 * @brief Initialize cingulate cortex subsystem
 *
 * WHAT: Creates cingulate adapter and connects all integration bridges
 * WHY:  Enable conflict monitoring and error detection in the brain
 * HOW:  Creates adapter, then quantum bridge, and connects to other systems
 *
 * BIOLOGICAL MOTIVATION:
 * - ACC is critical for error monitoring and cognitive control
 * - PCC is the hub of the Default Mode Network
 * - Damage causes problems with error awareness and behavioral adjustment
 *
 * INITIALIZATION ORDER:
 * 1. Create cingulate adapter (ACC + PCC modules)
 * 2. Create quantum bridge (accelerated conflict resolution)
 * 3. Connect to executive control
 * 4. Connect to emotional system
 * 5. Connect to autobiographical memory
 * 6. Connect to working memory
 * 7. Connect to immune system
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_cingulate_subsystem(brain_t brain);

/**
 * @brief Initialize cingulate quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for accelerated conflict resolution
 * WHY:  Conflict resolution benefits from parallel evaluation
 * HOW:  Uses Grover search for O(sqrt(N)) conflict arbitration
 *
 * APPLICATIONS:
 * - Response conflict: Find best resolution among competing options
 * - Error propagation: Quantum backpropagation of error signals
 * - Control optimization: Superposition of control levels
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_cingulate_quantum_bridge(brain_t brain);

/**
 * @brief Connect cingulate to executive control
 *
 * WHAT: Links cingulate to executive functions for cognitive control
 * WHY:  ACC generates control signals for prefrontal executive
 * HOW:  Registers cingulate as control signal source
 *
 * PATHWAYS:
 * - ACC -> DLPFC: Control demand signals
 * - ACC -> Motor cortex: Response threshold adjustment
 * - ACC -> Attention networks: Increased monitoring
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cingulate_to_executive(brain_t brain);

/**
 * @brief Connect cingulate to emotional system
 *
 * WHAT: Links cingulate to emotional processing
 * WHY:  Rostral ACC integrates emotion with cognition
 * HOW:  Registers for emotional state updates
 *
 * PATHWAYS:
 * - ACC <- Amygdala: Emotional salience
 * - ACC -> Emotional regulation: Control signals
 * - ACC -> Insula: Pain/distress processing
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cingulate_to_emotion(brain_t brain);

/**
 * @brief Connect cingulate to autobiographical memory
 *
 * WHAT: Links PCC to autobiographical memory system
 * WHY:  PCC mediates self-referential memory access
 * HOW:  Registers for autobiographical memory queries
 *
 * PATHWAYS:
 * - PCC <- Hippocampus: Memory retrieval
 * - PCC <- Medial temporal: Autobiographical details
 * - PCC -> Default Mode Network: Internal processing
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cingulate_to_autobio(brain_t brain);

/**
 * @brief Connect cingulate to working memory
 *
 * WHAT: Links cingulate to working memory for conflict resolution
 * WHY:  Conflict monitoring requires active representation of options
 * HOW:  Registers cingulate as working memory consumer
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cingulate_to_working_memory(brain_t brain);

/**
 * @brief Connect cingulate to immune system
 *
 * WHAT: Links cingulate to brain immune for inflammation effects
 * WHY:  Neuroinflammation affects error detection and control
 * HOW:  Registers for cytokine signals
 *
 * EFFECTS:
 * - High inflammation -> Reduced error detection sensitivity
 * - Cytokine storms -> Impaired cognitive control
 * - Recovery -> Gradual restoration of monitoring
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cingulate_to_immune(brain_t brain);

/**
 * @brief Connect cingulate to FEP orchestrator
 *
 * WHAT: Links cingulate to Free Energy Principle system
 * WHY:  ERN reflects prediction error; FEP is about minimizing it
 * HOW:  Registers cingulate as prediction error source
 *
 * INTEGRATION:
 * - ERN -> FEP: Error signals as prediction errors
 * - Control signal -> Active inference: Reduce future errors
 * - Conflict -> Free energy: Unresolved conflict = high free energy
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cingulate_to_fep(brain_t brain);

/**
 * @brief Shutdown cingulate subsystem
 *
 * WHAT: Cleanly shutdown cingulate and free resources
 * WHY:  Proper cleanup on brain destruction
 * HOW:  Destroy adapter and bridges in reverse order
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_shutdown_cingulate_subsystem(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_CINGULATE_H */
