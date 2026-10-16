//=============================================================================
// nimcp_brain_init_cerebellum.h - Cerebellum Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_cerebellum.h
 * @brief Cerebellum Initialization Functions
 *
 * WHAT: Initialization functions for Cerebellum (motor coordination)
 * WHY:  SRP refactoring - separate Cerebellum initialization logic
 * HOW:  Creates Cerebellum adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - "Little brain" at the back of the brainstem
 * - Motor coordination, timing, and error-based learning
 * - Purkinje cells receive input from parallel fibers (granule cells)
 * - Climbing fibers from inferior olive provide error signals
 * - Deep nuclei (dentate, interposed, fastigial) produce motor output
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of motor precision
 * - Thalamic Bridge: VL routing of motor commands to cortex
 * - Quantum Bridge: Grover-accelerated timing optimization
 *
 * @version Phase B4: Cerebellum Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_CEREBELLUM_H
#define NIMCP_BRAIN_INIT_CEREBELLUM_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Cerebellum Initialization API
//=============================================================================

/**
 * @brief Initialize Cerebellum subsystem
 *
 * WHAT: Creates Cerebellum adapter and connects all integration bridges
 * WHY:  Enable motor coordination capabilities in the brain
 * HOW:  Creates adapter, then substrate/thalamic/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Cerebellum is essential for precise motor control
 * - Damage causes ataxia (uncoordinated movements)
 * - Integrates sensory feedback with motor commands
 *
 * INITIALIZATION ORDER:
 * 1. Create Cerebellum adapter (granule, Purkinje, deep nuclei)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create thalamic bridge (signal routing)
 * 4. Create quantum bridge (accelerated timing)
 * 5. Connect to motor cortex
 * 6. Connect to basal ganglia
 * 7. Connect to brainstem
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_cerebellum_subsystem(brain_t brain);

/**
 * @brief Initialize Cerebellum's substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Motor precision depends on metabolic state (ATP, fatigue)
 * HOW:  Links Cerebellum to substrate for biologically-realistic control
 *
 * EFFECTS:
 * - Low ATP -> Reduced timing precision
 * - High fatigue -> Increased motor variability
 * - Stress -> Impaired error-based learning
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_cerebellum_substrate_bridge(brain_t brain);

/**
 * @brief Initialize Cerebellum's thalamic bridge
 *
 * WHAT: Creates bridge to thalamic router for signal routing
 * WHY:  Motor output requires thalamic coordination with motor cortex
 * HOW:  Routes motor commands through VL nucleus
 *
 * PATHWAYS:
 * - Motor commands -> VL -> Motor cortex (M1)
 * - Coordination signals -> VA -> Premotor cortex
 * - Balance signals -> VL -> Vestibular cortex
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_cerebellum_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize Cerebellum's quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for accelerated processing
 * WHY:  Timing optimization benefits from quantum speedup
 * HOW:  Uses Grover search for O(sqrt(N)) timing optimization
 *
 * APPLICATIONS:
 * - Timing optimization: Find optimal motor timing
 * - Trajectory evaluation: Compare multiple paths in parallel
 * - Gain adaptation: Optimize motor gains across DOF
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_cerebellum_quantum_bridge(brain_t brain);

/**
 * @brief Connect Cerebellum to motor cortex
 *
 * WHAT: Links Cerebellum to motor cortex for motor output
 * WHY:  Motor commands must be relayed to cortex for execution
 * HOW:  Registers Cerebellum as motor output source
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cerebellum_to_motor_cortex(brain_t brain);

/**
 * @brief Connect Cerebellum to basal ganglia
 *
 * WHAT: Links Cerebellum to basal ganglia for action coordination
 * WHY:  Motor selection and motor execution must be coordinated
 * HOW:  Bidirectional connection for shared motor planning
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cerebellum_to_basal_ganglia(brain_t brain);

/**
 * @brief Connect Cerebellum to brainstem
 *
 * WHAT: Links Cerebellum to brainstem for postural control
 * WHY:  Balance and posture require cerebellar-brainstem coordination
 * HOW:  Connects fastigial nucleus to vestibular nuclei
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cerebellum_to_brainstem(brain_t brain);

/**
 * @brief Connect Cerebellum to training system
 *
 * WHAT: Links Cerebellum to training for motor learning
 * WHY:  Motor skills improve through practice and error correction
 * HOW:  Registers Cerebellum adapter with training context
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cerebellum_to_training(brain_t brain);

/**
 * @brief Connect Cerebellum to immune system
 *
 * WHAT: Links Cerebellum to brain immune for inflammation modulation
 * WHY:  Neuroinflammation affects motor coordination
 * HOW:  Registers for cytokine signals that modulate precision
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_cerebellum_to_immune(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_CEREBELLUM_H */
