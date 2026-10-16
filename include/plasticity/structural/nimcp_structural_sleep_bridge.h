/**
 * @file nimcp_structural_sleep_bridge.h
 * @brief Sleep-Structural Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between sleep/wake system and structural plasticity
 * WHY:  Sleep orchestrates spine consolidation and pruning for memory storage
 * HOW:  Sleep states modulate formation, consolidation, and elimination dynamics
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → STRUCTURAL PLASTICITY PATHWAYS:
 * ---------------------------------------
 * 1. NREM Sleep Consolidation:
 *    - Deep NREM strengthens tagged spines
 *    - Synaptic upscaling of learning-related synapses
 *    - Spine stabilization through repeated reactivation
 *    - PSD growth and AMPAR insertion
 *    - Reference: Yang et al. (2014) "Sleep promotes branch-specific formation
 *      of dendritic spines after learning"
 *
 * 2. REM Sleep Pruning:
 *    - Selective elimination of weak spines
 *    - Synaptic downscaling (synaptic homeostasis hypothesis)
 *    - Refinement of neural circuits
 *    - Reference: Tononi & Cirelli (2014) "Sleep function and synaptic homeostasis"
 *
 * 3. Sleep Deprivation Effects:
 *    - Impaired spine formation
 *    - Reduced spine stabilization
 *    - Increased spine elimination
 *    - Memory consolidation deficits
 *    - Reference: Havekes et al. (2016) "Sleep deprivation causes memory deficits
 *      by negatively impacting neuronal connectivity in hippocampus"
 *
 * 4. Wake-Dependent Formation:
 *    - Awake state promotes spine formation
 *    - Learning drives nascent spine appearance
 *    - High activity triggers spinogenesis
 *    - Reference: Xu et al. (2009) "Rapid formation and selective stabilization
 *      of synapses for enduring motor memories"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-STRUCTURAL PLASTICITY INTEGRATION                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Formation  Consolidation  Pruning    Effect            ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   AWAKE            1.0        0.2            0.1        Active formation  ║
 * ║   DROWSY           0.5        0.3            0.2        Transition        ║
 * ║   LIGHT_NREM       0.1        1.0            0.3        Begin consol.     ║
 * ║   DEEP_NREM        0.0        2.0            0.1        Strong consol.    ║
 * ║   REM              0.0        0.1            1.5        Active pruning    ║
 * ║                                                                            ║
 * ║   CONSOLIDATION MECHANISM:                                                ║
 * ║   ┌────────────────────────────────────────────────────────┐              ║
 * ║   │  1. Tag spines during learning (awake)                 │              ║
 * ║   │  2. Reactivate tagged spines during NREM               │              ║
 * ║   │  3. Stabilize: NASCENT → STABLE                        │              ║
 * ║   │  4. Potentiate: STABLE → POTENTIATED                   │              ║
 * ║   └────────────────────────────────────────────────────────┘              ║
 * ║                                                                            ║
 * ║   PRUNING MECHANISM:                                                      ║
 * ║   ┌────────────────────────────────────────────────────────┐              ║
 * ║   │  1. Identify weak spines during REM                    │              ║
 * ║   │  2. Downscale synaptic strength                        │              ║
 * ║   │  3. Mark for elimination                               │              ║
 * ║   │  4. Remove if below threshold                          │              ║
 * ║   └────────────────────────────────────────────────────────┘              ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_STRUCTURAL_SLEEP_BRIDGE_H
#define NIMCP_STRUCTURAL_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/structural/nimcp_structural_plasticity.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Structural Modulation
 * ============================================================================ */

/* Formation rate modulation by sleep state */
#define STRUCTURAL_SLEEP_FORMATION_AWAKE        1.0f   /**< Full formation */
#define STRUCTURAL_SLEEP_FORMATION_DROWSY       0.5f   /**< Reduced formation */
#define STRUCTURAL_SLEEP_FORMATION_LIGHT_NREM   0.1f   /**< Minimal formation */
#define STRUCTURAL_SLEEP_FORMATION_DEEP_NREM    0.0f   /**< No formation */
#define STRUCTURAL_SLEEP_FORMATION_REM          0.0f   /**< No formation */

/* Consolidation boost by sleep state */
#define STRUCTURAL_SLEEP_CONSOLIDATION_AWAKE      0.2f   /**< Minimal consolidation */
#define STRUCTURAL_SLEEP_CONSOLIDATION_DROWSY     0.3f   /**< Slight boost */
#define STRUCTURAL_SLEEP_CONSOLIDATION_LIGHT_NREM 1.0f   /**< Active consolidation */
#define STRUCTURAL_SLEEP_CONSOLIDATION_DEEP_NREM  2.0f   /**< Strong consolidation */
#define STRUCTURAL_SLEEP_CONSOLIDATION_REM        0.1f   /**< Minimal consolidation */

/* Pruning rate modulation by sleep state */
#define STRUCTURAL_SLEEP_PRUNING_AWAKE         0.1f   /**< Minimal pruning */
#define STRUCTURAL_SLEEP_PRUNING_DROWSY        0.2f   /**< Slight increase */
#define STRUCTURAL_SLEEP_PRUNING_LIGHT_NREM    0.3f   /**< Moderate pruning */
#define STRUCTURAL_SLEEP_PRUNING_DEEP_NREM     0.1f   /**< Reduced pruning */
#define STRUCTURAL_SLEEP_PRUNING_REM           1.5f   /**< Active pruning */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-structural bridge configuration
 */
typedef struct {
    bool enable_sleep_consolidation;  /**< Enable NREM consolidation */
    bool enable_rem_pruning;          /**< Enable REM pruning */
    bool enable_formation_modulation; /**< Modulate formation by sleep state */
    float consolidation_strength;     /**< Overall consolidation multiplier */
    float pruning_strength;           /**< Overall pruning multiplier */
} structural_sleep_config_t;

/**
 * @brief Computed sleep effects on structural plasticity
 */
typedef struct {
    float formation_rate_factor;      /**< Multiply formation rate by this */
    float consolidation_boost;        /**< Consolidation strength multiplier */
    float pruning_rate_factor;        /**< Multiply pruning rate by this */
    sleep_state_t current_state;      /**< Current sleep state */
    float sleep_pressure;             /**< Current sleep pressure */
    bool active_consolidation;        /**< True during consolidation window */
    bool active_pruning;              /**< True during pruning window */
} structural_sleep_effects_t;

/**
 * @brief Sleep-structural integration bridge
 */
typedef struct structural_sleep_bridge_struct* structural_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-structural bridge configuration
 * WHY:  Provide sensible defaults based on sleep physiology
 */
int structural_sleep_default_config(structural_sleep_config_t* config);

/**
 * WHAT: Create sleep-structural bridge
 * WHY:  Initialize integration between sleep and structural plasticity systems
 */
structural_sleep_bridge_t structural_sleep_bridge_create(
    const structural_sleep_config_t* config,
    sleep_system_t sleep_system,
    structural_plasticity_system_t* structural_system
);

/**
 * WHAT: Destroy sleep-structural bridge
 * WHY:  Clean up resources and unregister callbacks
 */
void structural_sleep_bridge_destroy(structural_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update structural effects from sleep system state
 * WHY:  Compute how current sleep state affects spine dynamics
 */
int structural_sleep_update(structural_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated structural parameters for current sleep state
 * WHY:  Apply sleep modulation to spine formation/pruning
 */
int structural_sleep_get_effects(const structural_sleep_bridge_t bridge,
                                  structural_sleep_effects_t* effects);

/**
 * WHAT: Consolidate tagged spines during NREM sleep
 * WHY:  NREM sleep strengthens learning-related spines
 */
int structural_sleep_consolidate_tagged(structural_sleep_bridge_t bridge);

/**
 * WHAT: Prune weak spines during REM sleep
 * WHY:  REM sleep refines circuits through synaptic downscaling
 */
int structural_sleep_prune_weak(structural_sleep_bridge_t bridge);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float structural_sleep_get_formation_factor(sleep_state_t state);
float structural_sleep_get_consolidation_factor(sleep_state_t state);
float structural_sleep_get_pruning_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STRUCTURAL_SLEEP_BRIDGE_H */
