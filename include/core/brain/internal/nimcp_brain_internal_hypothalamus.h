/**
 * @file nimcp_brain_internal_hypothalamus.h
 * @brief Hypothalamus Internal Fields for brain_struct
 *
 * WHAT: Defines brain_struct fields for hypothalamus subsystem
 * WHY:  Modularize brain_internal.h - separate hypothalamus fields
 * HOW:  Included by nimcp_brain_internal.h to compose brain_struct
 *
 * BIOLOGICAL BASIS:
 * - Hypothalamus is the master regulator of homeostasis
 * - Controls temperature, hunger, thirst, circadian rhythms, stress
 * - Key nuclei: SCN (circadian), PVN (HPA axis), LH (arousal), VMH (satiety)
 * - Connections to pituitary (neuroendocrine), brainstem (autonomic), limbic
 *
 * INTEGRATION BRIDGES:
 * - Limbic Bridge: Emotional input from amygdala
 * - Brainstem Bridge: Autonomic output to medulla
 * - Pituitary Bridge: Neuroendocrine output
 * - Quantum Bridge: Quantum-optimized homeostatic regulation
 *
 * REFACTORING HISTORY:
 * - Created as part of Phase H1: Hypothalamus Brain Integration
 *
 * @version Phase H1: Hypothalamus Brain Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_BRAIN_INTERNAL_HYPOTHALAMUS_H
#define NIMCP_BRAIN_INTERNAL_HYPOTHALAMUS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * Forward Declarations for Hypothalamus Types
 *===========================================================================*/

/* Core hypothalamus adapter */
struct hypothalamus_adapter;
typedef struct hypothalamus_adapter hypothalamus_adapter_t;

/* Integration bridges */
struct hypothalamus_limbic_bridge;
typedef struct hypothalamus_limbic_bridge hypothalamus_limbic_bridge_t;

struct hypothalamus_brainstem_bridge;
typedef struct hypothalamus_brainstem_bridge hypothalamus_brainstem_bridge_t;

struct hypothalamus_pituitary_bridge;
typedef struct hypothalamus_pituitary_bridge hypothalamus_pituitary_bridge_t;

struct hypothalamus_quantum_bridge;
typedef struct hypothalamus_quantum_bridge hypothalamus_quantum_bridge_t;

/*=============================================================================
 * Hypothalamus Fields for brain_struct
 *===========================================================================*/

/**
 * HYPOTHALAMUS INTEGRATION (Homeostatic Regulation)
 *
 * The Hypothalamus provides homeostatic regulation capabilities:
 * - Circadian Rhythms: SCN-driven 24-hour biological clock
 * - Temperature Regulation: Core body temperature homeostasis
 * - Hunger/Satiety: Appetite control via arcuate nucleus
 * - Thirst/Osmolality: Fluid balance via osmoreceptors
 * - Stress Response: HPA axis (CRH → ACTH → Cortisol)
 * - Autonomic Control: Sympathetic/parasympathetic balance
 *
 * The Hypothalamus adapter unifies:
 * - SCN: Suprachiasmatic nucleus (circadian pacemaker)
 * - PVN: Paraventricular nucleus (HPA axis, autonomic)
 * - LH: Lateral hypothalamus (arousal, feeding)
 * - VMH: Ventromedial hypothalamus (satiety, defense)
 * - Arcuate: Appetite regulation (NPY, POMC)
 *
 * Integrates with:
 * - Limbic System: Emotional influence on stress response
 * - Brainstem/Medulla: Autonomic output
 * - Pituitary: Neuroendocrine output (hormone release)
 * - Sleep System: Circadian-sleep coupling
 * - Immune System: Sickness behavior, fever
 * - Wellbeing: Stress-distress integration
 * - Emotional System: Stress-emotion coupling
 * - Quantum Reasoner: Optimized homeostatic regulation
 *
 * FIELD NAMING CONVENTION:
 * - hypothalamus: Core adapter
 * - hypothalamus_*_bridge: Integration bridges to other subsystems
 * - hypothalamus_enabled: Master enable flag
 * - last_hypothalamus_update_us: Update timestamp for timing control
 */

/**
 * @brief Macro defining hypothalamus fields for brain_struct
 *
 * USAGE: Include this macro in brain_struct definition
 * WHY:   Enables modular composition of brain_struct from separate headers
 *
 * FIELDS:
 * - hypothalamus: Core adapter for circadian/homeostatic/HPA/autonomic
 * - hypothalamus_limbic_bridge: Emotional input from amygdala
 * - hypothalamus_brainstem_bridge: Autonomic output to medulla/pons
 * - hypothalamus_pituitary_bridge: Neuroendocrine output to pituitary
 * - hypothalamus_quantum_bridge: Quantum-optimized homeostatic regulation
 * - hypothalamus_enabled: Master enable flag for hypothalamus subsystem
 * - last_hypothalamus_update_us: Timestamp for update rate limiting
 */
#define BRAIN_INTERNAL_FIELDS_HYPOTHALAMUS                                      \
    /* === HYPOTHALAMUS INTEGRATION (Homeostatic Regulation) === */             \
    /*                                                                          \
     * The Hypothalamus is the master regulator of homeostasis:                 \
     * - Circadian rhythms (SCN drives sleep/wake cycle)                        \
     * - Temperature regulation (thermostat)                                    \
     * - Hunger and satiety (appetite control)                                  \
     * - Thirst and fluid balance (osmotic regulation)                          \
     * - Stress response (HPA axis → cortisol)                                  \
     * - Autonomic balance (sympathetic/parasympathetic)                        \
     *                                                                          \
     * BIOLOGICAL BASIS:                                                        \
     * The hypothalamus contains multiple nuclei with specialized functions:    \
     * - SCN: Suprachiasmatic nucleus (circadian pacemaker)                     \
     * - PVN: Paraventricular nucleus (HPA axis, oxytocin, vasopressin)         \
     * - LH: Lateral hypothalamus (arousal, feeding, orexin)                    \
     * - VMH: Ventromedial hypothalamus (satiety, defensive behavior)           \
     * - Arcuate: Appetite hormones (NPY hunger, POMC satiety)                  \
     * - SON: Supraoptic nucleus (vasopressin, thirst)                          \
     */                                                                         \
    hypothalamus_adapter_t* hypothalamus;                       /* Core hypothalamus adapter */ \
    hypothalamus_limbic_bridge_t* hypothalamus_limbic_bridge;   /* Limbic (emotional) input */ \
    hypothalamus_brainstem_bridge_t* hypothalamus_brainstem_bridge; /* Brainstem output */ \
    hypothalamus_pituitary_bridge_t* hypothalamus_pituitary_bridge; /* Neuroendocrine output */ \
    hypothalamus_quantum_bridge_t* hypothalamus_quantum_bridge; /* Quantum optimization */ \
    bool hypothalamus_enabled;                                  /* Hypothalamus enabled */ \
    uint64_t last_hypothalamus_update_us;                       /* Last update timestamp */

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_INTERNAL_HYPOTHALAMUS_H */
