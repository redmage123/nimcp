//=============================================================================
// nimcp_brain_init_prefrontal.h - Prefrontal Cortex Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_prefrontal.h
 * @brief Prefrontal Cortex Initialization Functions
 *
 * WHAT: Initialization functions for prefrontal cortex (executive functions)
 * WHY:  SRP refactoring - separate prefrontal initialization logic
 * HOW:  Creates prefrontal adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Brodmann areas 9, 10, 11, 44, 45, 46, 47
 * - Dorsolateral PFC (BA9/46): Working memory, cognitive control
 * - Ventromedial PFC (BA10/11): Decision-making, value-based choice
 * - Orbitofrontal Cortex (BA11/47): Reward processing, impulse control
 * - Anterior Cingulate (BA32): Conflict monitoring, error detection
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of executive function
 * - Thalamic Bridge: MD routing of executive commands
 * - Quantum Bridge: Quantum-accelerated decision-making and planning
 *
 * CONNECTIONS:
 * - Working Memory: Active maintenance of goals and information
 * - Basal Ganglia: Action selection and motor planning
 * - Thalamus: Signal routing and gating
 * - Amygdala: Emotional modulation of decisions
 * - Hippocampus: Memory-based decision support
 *
 * @version Phase PFC-1: Prefrontal Cortex Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_PREFRONTAL_H
#define NIMCP_BRAIN_INIT_PREFRONTAL_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Prefrontal Cortex Initialization API
//=============================================================================

/**
 * @brief Initialize prefrontal cortex subsystem
 *
 * WHAT: Creates prefrontal adapter and connects all integration bridges
 * WHY:  Enable executive function capabilities in the brain
 * HOW:  Creates adapter, then substrate/thalamic/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Prefrontal cortex is essential for executive function
 * - Damage causes dysexecutive syndrome (planning/inhibition deficits)
 * - Integrates goals, working memory, decision-making, impulse control
 *
 * INITIALIZATION ORDER:
 * 1. Create prefrontal adapter (core executive functions)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create thalamic bridge (signal routing)
 * 4. Create quantum bridge (accelerated decision-making)
 * 5. Connect to working memory
 * 6. Connect to basal ganglia
 * 7. Connect to immune system
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_prefrontal_subsystem(brain_t brain);

/**
 * @brief Initialize prefrontal substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Executive function depends on metabolic state (ATP, fatigue)
 * HOW:  Links prefrontal to substrate for biologically-realistic processing
 *
 * EFFECTS:
 * - Low ATP: Reduced working memory capacity
 * - High fatigue: Impaired impulse control
 * - Stress: Disrupted planning and decision-making
 * - Glucose: Modulates cognitive effort capacity
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_prefrontal_substrate_bridge(brain_t brain);

/**
 * @brief Initialize prefrontal thalamic bridge
 *
 * WHAT: Creates bridge to thalamic router for signal routing
 * WHY:  Executive function requires thalamic coordination with cortex
 * HOW:  Routes executive commands through MD nucleus
 *
 * PATHWAYS:
 * - Executive commands: MD -> Prefrontal cortex (loop)
 * - Motor planning: VA/VL -> Motor cortex
 * - Attention signals: Pulvinar -> Parietal/temporal
 * - Memory access: Anterior -> Hippocampus
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_prefrontal_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize prefrontal quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for accelerated processing
 * WHY:  Decision-making benefits from quantum speedup
 * HOW:  Uses Grover search for O(sqrt(N)) decision evaluation
 *
 * APPLICATIONS:
 * - Decision acceleration: Parallel option evaluation
 * - Planning optimization: Constraint satisfaction
 * - Conflict resolution: Multi-objective optimization
 * - Probability estimation: Amplitude estimation for outcomes
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_prefrontal_quantum_bridge(brain_t brain);

/**
 * @brief Connect prefrontal cortex to working memory
 *
 * WHAT: Links prefrontal to working memory for goal maintenance
 * WHY:  Executive function requires active information maintenance
 * HOW:  Registers prefrontal as WM controller for executive slots
 *
 * INTEGRATION:
 * - Goal representation: Maintained in dorsolateral PFC
 * - Task rules: Encoded in prefrontal-parietal circuit
 * - Context information: Held for decision-making
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_prefrontal_to_working_memory(brain_t brain);

/**
 * @brief Connect prefrontal cortex to basal ganglia
 *
 * WHAT: Links prefrontal to basal ganglia for action selection
 * WHY:  Executive function coordinates with motor system
 * HOW:  Establishes prefrontal-striatal-thalamic loop
 *
 * PATHWAYS:
 * - Direct pathway: Go signals for selected actions
 * - Indirect pathway: NoGo signals for inhibited actions
 * - Hyperdirect pathway: Fast inhibition from prefrontal
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_prefrontal_to_basal_ganglia(brain_t brain);

/**
 * @brief Connect prefrontal cortex to thalamus
 *
 * WHAT: Links prefrontal to thalamus for signal routing
 * WHY:  Thalamus gates information flow to/from prefrontal
 * HOW:  Establishes reciprocal prefrontal-thalamic connections
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_prefrontal_to_thalamus(brain_t brain);

/**
 * @brief Connect prefrontal cortex to training system
 *
 * WHAT: Links prefrontal to training for executive function learning
 * WHY:  Decision-making improves through experience
 * HOW:  Registers prefrontal adapter with training context
 *
 * LEARNING:
 * - Value learning: Expected values of actions
 * - Rule learning: Task contingencies
 * - Strategy learning: Optimal decision policies
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_prefrontal_to_training(brain_t brain);

/**
 * @brief Connect prefrontal cortex to immune system
 *
 * WHAT: Links prefrontal to brain immune for inflammation modulation
 * WHY:  Neuroinflammation affects executive function
 * HOW:  Registers for cytokine signals that modulate cognition
 *
 * EFFECTS:
 * - IL-1beta: Impaired working memory
 * - TNF-alpha: Reduced cognitive flexibility
 * - IL-6: Decreased planning capacity
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_prefrontal_to_immune(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_PREFRONTAL_H */
