/**
 * @file nimcp_brain_init_parietal_cortex.h
 * @brief Parietal Cortex Region Initialization Functions
 *
 * WHAT: Initialization functions for parietal cortex region (spatial/sensorimotor)
 * WHY:  SRP refactoring - separate parietal cortex initialization logic
 * HOW:  Creates parietal adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Posterior parietal cortex (Brodmann areas 5, 7, 39, 40)
 * - Primary somatosensory cortex (S1) - areas 3a, 3b, 1, 2
 * - Secondary somatosensory cortex (S2) - area 40
 * - Superior parietal lobule (SPL) - spatial attention
 * - Inferior parietal lobule (IPL) - sensorimotor integration
 * - Intraparietal sulcus (IPS) - reaching, grasping, numerical
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: ATP/fatigue modulation of spatial processing
 * - Thalamic Bridge: Pulvinar/LP routing of spatial signals
 * - Quantum Bridge: Superposition for attention allocation
 *
 * @version Phase PC1: Parietal Cortex Brain Integration
 * @author NIMCP Development Team
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_PARIETAL_CORTEX_H
#define NIMCP_BRAIN_INIT_PARIETAL_CORTEX_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations
 *===========================================================================*/

struct parietal_adapter;
typedef struct parietal_adapter parietal_adapter_t;

struct parietal_quantum_bridge;
typedef struct parietal_quantum_bridge parietal_quantum_bridge_t;

/*=============================================================================
 * Parietal Cortex Initialization API
 *===========================================================================*/

/**
 * @brief Initialize parietal cortex subsystem
 *
 * WHAT: Creates parietal adapter and connects all integration bridges
 * WHY:  Enable spatial processing capabilities in the brain
 * HOW:  Creates adapter, then substrate/thalamic/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Parietal cortex is essential for spatial awareness
 * - Damage causes spatial neglect, reaching deficits
 * - Integrates somatosensory, visual, and motor information
 *
 * INITIALIZATION ORDER:
 * 1. Create Parietal adapter (somatosensory/spatial/sensorimotor)
 * 2. Create substrate bridge (metabolic modulation)
 * 3. Create thalamic bridge (signal routing)
 * 4. Create quantum bridge (attention superposition)
 * 5. Connect to motor cortex
 * 6. Connect to visual cortex
 * 7. Connect to frontal cortex
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_parietal_cortex_subsystem(brain_t brain);

/**
 * @brief Initialize parietal cortex substrate bridge
 *
 * WHAT: Creates bridge to neural substrate for metabolic modulation
 * WHY:  Spatial processing depends on metabolic state (ATP, fatigue)
 * HOW:  Links parietal cortex to substrate for biologically-realistic processing
 *
 * EFFECTS:
 * - Low ATP: Reduced spatial acuity
 * - High fatigue: Simplified attention, slower coordination
 * - Stress: Spatial disorientation
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_parietal_cortex_substrate_bridge(brain_t brain);

/**
 * @brief Initialize parietal cortex thalamic bridge
 *
 * WHAT: Creates bridge to thalamic router for signal routing
 * WHY:  Spatial processing requires thalamic coordination
 * HOW:  Routes spatial signals through pulvinar and LP nuclei
 *
 * PATHWAYS:
 * - Visual spatial: Pulvinar to parietal
 * - Somatosensory: VPL/VPM to S1/S2
 * - Motor feedback: LP to sensorimotor areas
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_parietal_cortex_thalamic_bridge(brain_t brain);

/**
 * @brief Initialize parietal cortex quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for accelerated processing
 * WHY:  Spatial attention benefits from quantum parallelism
 * HOW:  Uses superposition for multi-target attention, quantum walk for navigation
 *
 * APPLICATIONS:
 * - Spatial attention: Grover search for salient locations
 * - Coordinate transforms: Superposition across reference frames
 * - Motor planning: Trajectory optimization
 * - Navigation: Quantum walk on spatial graph
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_parietal_cortex_quantum_bridge(brain_t brain);

/**
 * @brief Connect parietal cortex to motor cortex
 *
 * WHAT: Links parietal to motor cortex for sensorimotor integration
 * WHY:  Reaching and grasping require parietal-motor coordination
 * HOW:  Registers motor plan callback for parietal output
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_parietal_to_motor(brain_t brain);

/**
 * @brief Connect parietal cortex to visual cortex
 *
 * WHAT: Links parietal to visual cortex for visuospatial processing
 * WHY:  Spatial attention modulates visual processing
 * HOW:  Registers attention callback for visual selection
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_parietal_to_visual(brain_t brain);

/**
 * @brief Connect parietal cortex to frontal cortex
 *
 * WHAT: Links parietal to prefrontal for executive spatial control
 * WHY:  Spatial planning requires frontal-parietal integration
 * HOW:  Bidirectional connection for goal-directed spatial behavior
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_parietal_to_frontal(brain_t brain);

/**
 * @brief Connect parietal cortex to working memory
 *
 * WHAT: Links parietal to working memory for spatial representations
 * WHY:  Spatial working memory requires parietal maintenance
 * HOW:  Registers parietal as WM consumer for spatial slots
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_parietal_to_working_memory(brain_t brain);

/**
 * @brief Connect parietal cortex to training system
 *
 * WHAT: Links parietal to training for sensorimotor learning
 * WHY:  Motor skills improve through practice and feedback
 * HOW:  Registers parietal adapter with training context
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_parietal_to_training(brain_t brain);

/**
 * @brief Connect parietal cortex to immune system
 *
 * WHAT: Links parietal to brain immune for inflammation modulation
 * WHY:  Neuroinflammation affects spatial processing
 * HOW:  Registers for cytokine signals that modulate spatial acuity
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_parietal_to_immune(brain_t brain);

/*=============================================================================
 * Runtime Functions
 *===========================================================================*/

/**
 * @brief Get parietal cortex adapter from brain
 *
 * @param brain Brain instance
 * @return Parietal adapter or NULL if not enabled
 */
parietal_adapter_t* brain_get_parietal_cortex(brain_t brain);

/**
 * @brief Get parietal cortex quantum bridge from brain
 *
 * @param brain Brain instance
 * @return Quantum bridge or NULL if not enabled
 */
parietal_quantum_bridge_t* brain_get_parietal_cortex_quantum_bridge(brain_t brain);

/**
 * @brief Step parietal cortex processing
 *
 * @param brain Brain instance
 * @param delta_t Time step in microseconds
 * @return 0 on success, -1 on error
 */
int brain_step_parietal_cortex(brain_t brain, uint64_t delta_t);

/**
 * @brief Update parietal from immune system state
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_update_parietal_cortex_from_immune(brain_t brain);

/**
 * @brief Update parietal from sleep/fatigue state
 *
 * @param brain Brain instance
 * @return 0 on success, -1 on error
 */
int brain_update_parietal_cortex_from_sleep(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_PARIETAL_CORTEX_H */
