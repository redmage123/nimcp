//=============================================================================
// nimcp_brain_init_motor.h - Motor Cortex Initialization
//=============================================================================
/**
 * @file nimcp_brain_init_motor.h
 * @brief Motor Cortex Initialization Functions
 *
 * WHAT: Initialization functions for Motor Cortex (M1, premotor, SMA)
 * WHY:  SRP refactoring - separate motor initialization logic
 * HOW:  Creates motor adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Brodmann area 4 (primary motor cortex, M1)
 * - Brodmann area 6 (premotor and supplementary motor areas)
 * - Corticospinal tract for voluntary movement
 * - Somatotopic organization (motor homunculus)
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of motor output
 * - Thalamic Bridge: VA/VL routing of motor commands
 * - Quantum Bridge: Trajectory optimization and parallel program evaluation
 *
 * @version Phase M1: Motor Cortex Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_MOTOR_H
#define NIMCP_BRAIN_INIT_MOTOR_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Motor Cortex Initialization API
//=============================================================================

/**
 * @brief Initialize Motor Cortex subsystem
 *
 * WHAT: Creates motor adapter and connects all integration bridges
 * WHY:  Enable motor planning and execution capabilities in the brain
 * HOW:  Creates adapter, then substrate/thalamic/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Motor cortex is essential for voluntary movement
 * - Damage causes motor deficits (hemiparesis, apraxia)
 * - Integrates with basal ganglia, cerebellum, and sensory cortices
 *
 * INITIALIZATION ORDER:
 * 1. Create Motor adapter (core M1/premotor/SMA processors)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create thalamic bridge (signal routing through VA/VL)
 * 4. Create quantum bridge (trajectory optimization)
 * 5. Connect to basal ganglia (action selection)
 * 6. Connect to cerebellum (motor coordination)
 * 7. Connect to training system (motor learning)
 * 8. Connect to immune system (inflammation effects)
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_motor_subsystem(brain_t brain);

/**
 * @brief Initialize Motor substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Motor output depends on metabolic state (ATP, fatigue)
 * HOW:  Links motor cortex to substrate for biologically-realistic execution
 *
 * EFFECTS:
 * - Low ATP -> Reduced movement speed and force
 * - High fatigue -> Movement inaccuracy, tremor
 * - Stress -> Increased motor variability
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_motor_substrate_bridge(brain_t brain);

/**
 * @brief Initialize Motor thalamic bridge
 *
 * WHAT: Creates bridge to thalamic router for signal routing
 * WHY:  Motor commands require thalamic coordination
 * HOW:  Routes motor commands through VA/VL nuclei
 *
 * PATHWAYS:
 * - Motor commands -> VA/VL -> Primary motor cortex (M1)
 * - Cerebellar feedback -> VL -> Premotor cortex
 * - Basal ganglia output -> VA -> SMA
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_motor_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize Motor quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for optimized planning
 * WHY:  Trajectory optimization benefits from quantum speedup
 * HOW:  Uses Grover search for O(sqrt(N)) trajectory selection
 *
 * APPLICATIONS:
 * - Trajectory optimization: Find smoothest/fastest path
 * - Motor program selection: Choose most appropriate skill
 * - Movement timing: Optimize temporal coordination
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_motor_quantum_bridge(brain_t brain);

/**
 * @brief Connect Motor Cortex to basal ganglia
 *
 * WHAT: Links motor cortex to basal ganglia for action selection
 * WHY:  BG determines which motor programs to execute
 * HOW:  Registers motor cortex as recipient of BG action signals
 *
 * INTEGRATION:
 * - Direct pathway: Facilitates selected movements
 * - Indirect pathway: Suppresses competing movements
 * - Hyperdirect pathway: Fast stopping/switching
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_motor_to_basal_ganglia(brain_t brain);

/**
 * @brief Connect Motor Cortex to cerebellum
 *
 * WHAT: Links motor cortex to cerebellum for motor coordination
 * WHY:  Cerebellum provides timing, accuracy, and motor learning
 * HOW:  Registers for cerebellar correction signals
 *
 * CEREBELLAR FUNCTIONS:
 * - Timing: Precise temporal coordination of movements
 * - Error correction: Climbing fiber teaching signals
 * - Motor adaptation: Calibration of motor commands
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_motor_to_cerebellum(brain_t brain);

/**
 * @brief Connect Motor Cortex to thalamus
 *
 * WHAT: Links motor cortex to thalamic router
 * WHY:  Thalamus gates and routes motor signals
 * HOW:  Registers for thalamic gating modulation
 *
 * THALAMIC NUCLEI:
 * - VA (ventral anterior): BG motor loop
 * - VL (ventral lateral): Cerebellar motor loop
 * - VPL (ventral posterolateral): Somatosensory feedback
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_motor_to_thalamus(brain_t brain);

/**
 * @brief Connect Motor Cortex to training system
 *
 * WHAT: Links motor cortex to training for motor learning
 * WHY:  Motor skills improve through practice and feedback
 * HOW:  Registers motor adapter with training context
 *
 * LEARNING TYPES:
 * - Error-based learning: Correct movements from feedback
 * - Reinforcement learning: Shape movements via reward
 * - Imitation learning: Learn from demonstrations
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_motor_to_training(brain_t brain);

/**
 * @brief Connect Motor Cortex to immune system
 *
 * WHAT: Links motor cortex to brain immune for inflammation modulation
 * WHY:  Neuroinflammation affects motor function
 * HOW:  Registers for cytokine signals that modulate motor output
 *
 * INFLAMMATORY EFFECTS:
 * - IL-1beta: Reduced motor speed
 * - TNF-alpha: Increased motor variability
 * - IL-6: Fatigue-like motor slowing
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_motor_to_immune(brain_t brain);

/**
 * @brief Update Motor Cortex subsystem
 *
 * WHAT: Performs periodic update of motor subsystem
 * WHY:  Motor execution requires continuous updates
 * HOW:  Calls motor update functions at appropriate rate
 *
 * UPDATE OPERATIONS:
 * - Process pending movements
 * - Update execution state
 * - Apply sensory feedback corrections
 * - Update bridge states
 *
 * @param brain Brain instance
 * @param dt_us Time since last update in microseconds
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_update_motor_subsystem(brain_t brain, uint64_t dt_us);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_MOTOR_H */
