/**
 * @file nimcp_brain_init_hypothalamus.h
 * @brief Hypothalamus Initialization Functions
 *
 * WHAT: Initialization functions for hypothalamus (homeostatic regulation)
 * WHY:  SRP refactoring - separate hypothalamus initialization logic
 * HOW:  Creates hypothalamus adapter and connects all integration bridges
 *
 * BIOLOGICAL BASIS:
 * - Hypothalamus is the master regulator of homeostasis
 * - Controls temperature, hunger, thirst, circadian rhythms, stress
 * - Key nuclei: SCN (circadian), PVN (HPA axis), LH (arousal), VMH (satiety)
 * - Connections to pituitary (neuroendocrine), brainstem (autonomic), limbic
 *
 * INTEGRATION BRIDGES:
 * - Substrate Bridge: Metabolic state modulation of homeostatic setpoints
 * - Limbic Bridge: Emotional influence on stress response
 * - Brainstem Bridge: Autonomic output to medulla
 * - Quantum Bridge: Quantum-optimized homeostatic regulation
 *
 * @version Phase H1: Hypothalamus Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INIT_HYPOTHALAMUS_H
#define NIMCP_BRAIN_INIT_HYPOTHALAMUS_H

#include "core/brain/nimcp_brain.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Hypothalamus Initialization API
 *===========================================================================*/

/**
 * @brief Initialize hypothalamus subsystem
 *
 * WHAT: Creates hypothalamus adapter and connects all integration bridges
 * WHY:  Enable homeostatic regulation capabilities in the brain
 * HOW:  Creates adapter, then limbic/brainstem/pituitary/quantum bridges
 *
 * BIOLOGICAL MOTIVATION:
 * - Hypothalamus is essential for survival (temperature, hunger, thirst)
 * - Damage causes dysautonomia, circadian disruption, hormonal imbalance
 * - Integrates internal state with behavioral drives
 *
 * INITIALIZATION ORDER:
 * 1. Create hypothalamus adapter (core circadian/homeostatic/HPA/autonomic)
 * 2. Create limbic bridge (emotional input from amygdala)
 * 3. Create brainstem bridge (autonomic output to medulla)
 * 4. Create pituitary bridge (neuroendocrine output)
 * 5. Create quantum bridge (optimized regulation)
 * 6. Connect to sleep/wake system
 * 7. Connect to immune system
 * 8. Connect to wellbeing monitor
 *
 * @param brain Brain instance to initialize
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_hypothalamus_subsystem(brain_t brain);

/**
 * @brief Initialize hypothalamus limbic bridge
 *
 * WHAT: Creates bridge to limbic system for emotional input
 * WHY:  Emotional state affects stress response and homeostasis
 * HOW:  Links hypothalamus to amygdala for threat/stress signaling
 *
 * EFFECTS:
 * - Fear/anxiety → Increased HPA activation
 * - Chronic stress → Altered setpoints, metabolic effects
 * - Positive emotions → Reduced cortisol, improved homeostasis
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_hypothalamus_limbic_bridge(brain_t brain);

/**
 * @brief Initialize hypothalamus brainstem bridge
 *
 * WHAT: Creates bridge to brainstem for autonomic output
 * WHY:  Hypothalamus controls autonomic nervous system via medulla
 * HOW:  Routes autonomic commands through brainstem nuclei
 *
 * PATHWAYS:
 * - Sympathetic output → NTS → Cardiovascular control
 * - Parasympathetic output → DMV → Digestive control
 * - Respiratory control → Pre-Botzinger complex
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_hypothalamus_brainstem_bridge(brain_t brain);

/**
 * @brief Initialize hypothalamus pituitary bridge
 *
 * WHAT: Creates bridge to pituitary for neuroendocrine output
 * WHY:  Hypothalamus controls hormones via pituitary portal system
 * HOW:  Routes releasing hormones to anterior pituitary
 *
 * HORMONES:
 * - CRH → ACTH → Cortisol (stress response)
 * - TRH → TSH → T3/T4 (metabolism)
 * - GnRH → FSH/LH (reproduction)
 * - GHRH → GH (growth)
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_hypothalamus_pituitary_bridge(brain_t brain);

/**
 * @brief Initialize hypothalamus quantum bridge
 *
 * WHAT: Creates bridge to quantum reasoner for optimized regulation
 * WHY:  Multi-objective homeostasis benefits from quantum optimization
 * HOW:  Uses quantum annealing for setpoint optimization
 *
 * APPLICATIONS:
 * - Homeostatic optimization: Balance temperature/hunger/thirst
 * - Circadian optimization: Phase adjustment for jet lag
 * - HPA tuning: Optimal stress response parameters
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_init_hypothalamus_quantum_bridge(brain_t brain);

/**
 * @brief Connect hypothalamus to sleep/wake system
 *
 * WHAT: Links hypothalamus to sleep system for circadian integration
 * WHY:  SCN drives sleep/wake cycle, sleep affects homeostasis
 * HOW:  Registers hypothalamus as circadian time source
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hypothalamus_to_sleep(brain_t brain);

/**
 * @brief Connect hypothalamus to immune system
 *
 * WHAT: Links hypothalamus to brain immune for cytokine signaling
 * WHY:  Inflammation affects body temperature, sickness behavior
 * HOW:  Registers for cytokine signals that modulate setpoints
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hypothalamus_to_immune(brain_t brain);

/**
 * @brief Connect hypothalamus to wellbeing monitor
 *
 * WHAT: Links hypothalamus to wellbeing for stress/distress integration
 * WHY:  Chronic stress affects wellbeing, wellbeing affects HPA axis
 * HOW:  Registers for distress signals and provides stress metrics
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hypothalamus_to_wellbeing(brain_t brain);

/**
 * @brief Connect hypothalamus to medulla
 *
 * WHAT: Links hypothalamus to medulla oblongata for arousal integration
 * WHY:  Medulla arousal state affects hypothalamic regulation
 * HOW:  Bidirectional connection for autonomic coordination
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hypothalamus_to_medulla(brain_t brain);

/**
 * @brief Connect hypothalamus to emotional system
 *
 * WHAT: Links hypothalamus to emotional system for stress-emotion coupling
 * WHY:  Emotions drive stress response, stress affects emotional state
 * HOW:  Registers for emotional state changes, provides stress signals
 *
 * @param brain Brain instance
 * @return true on success, false on failure
 */
bool nimcp_brain_factory_connect_hypothalamus_to_emotions(brain_t brain);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INIT_HYPOTHALAMUS_H */
