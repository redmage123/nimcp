/**
 * @file nimcp_protein_sleep_bridge.h
 * @brief Sleep-Protein Synthesis Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between sleep/wake system and protein synthesis
 * WHY:  Sleep dramatically upregulates protein synthesis needed for memory consolidation
 * HOW:  Sleep state modulates PRP synthesis rate, deep NREM provides maximum synthesis
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → PROTEIN SYNTHESIS PATHWAYS:
 * ------------------------------------
 * 1. Sleep-Dependent Protein Synthesis Upregulation:
 *    - AWAKE: Low synthesis (metabolic cost, competing demands)
 *    - DROWSY: Slight increase (transition to restorative mode)
 *    - LIGHT_NREM: Moderate synthesis (preparation for consolidation)
 *    - DEEP_NREM: Maximum synthesis (2-3x wake levels)
 *    - REM: High synthesis with enhanced delivery to synapses
 *    - Reference: Rasch & Born (2013) "About sleep's role in memory"
 *
 * 2. Sleep Deprivation Effects:
 *    - Blocks protein synthesis required for consolidation
 *    - Tags set during wake persist but cannot capture PRPs
 *    - Memory consolidation fails without sleep
 *    - Reference: Diekelmann & Born (2010) "Memory consolidation during sleep"
 *
 * 3. Deep Sleep Consolidation Window:
 *    - Slow wave activity (0.5-4Hz) triggers synthesis
 *    - Tags from previous wake period capture PRPs
 *    - L-LTP achieved during deep sleep
 *    - Reference: Marshall & Born (2007) "Slow oscillations in consolidation"
 *
 * 4. REM Sleep Protein Delivery:
 *    - High synthesis continues
 *    - Enhanced diffusion to dendritic compartments
 *    - Supports consolidation of emotional/procedural memories
 *    - Reference: Sara (2017) "Sleep to remember"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-PROTEIN SYNTHESIS INTEGRATION BRIDGE                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      PRP Synthesis    Effect                                ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   AWAKE            1.0x             Baseline synthesis                    ║
 * ║   DROWSY           1.2x             Slight increase                       ║
 * ║   LIGHT_NREM       1.5x             Moderate increase                     ║
 * ║   DEEP_NREM        2.5x             Maximum synthesis                     ║
 * ║   REM              1.8x             High synthesis + delivery             ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PROTEIN_SLEEP_BRIDGE_H
#define NIMCP_PROTEIN_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/protein/nimcp_protein_synthesis.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Protein Synthesis Modulation
 * ============================================================================ */

/* Protein synthesis rate modulation by sleep state */
#define PROTEIN_SLEEP_SYNTH_AWAKE_FACTOR        1.0f   /**< Baseline synthesis */
#define PROTEIN_SLEEP_SYNTH_DROWSY_FACTOR       1.2f   /**< Slight increase */
#define PROTEIN_SLEEP_SYNTH_LIGHT_NREM_FACTOR   1.5f   /**< Moderate increase */
#define PROTEIN_SLEEP_SYNTH_DEEP_NREM_FACTOR    2.5f   /**< Maximum synthesis */
#define PROTEIN_SLEEP_SYNTH_REM_FACTOR          1.8f   /**< High synthesis */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-protein synthesis bridge configuration
 */
typedef struct {
    bool enable_synthesis_modulation;  /**< Enable synthesis rate changes */
    bool enable_delivery_modulation;   /**< Enable REM delivery enhancement */
    float modulation_strength;         /**< Overall modulation strength (0-1) */
} protein_sleep_config_t;

/**
 * @brief Computed sleep effects on protein synthesis
 */
typedef struct {
    float synthesis_rate_factor;       /**< Multiply synthesis rate by this */
    float delivery_enhancement;        /**< PRP delivery efficiency boost */
    sleep_state_t current_state;       /**< Current sleep state */
    float sleep_pressure;              /**< Current sleep pressure */
    bool deep_sleep_consolidation;     /**< True during deep NREM */
} protein_sleep_effects_t;

/**
 * @brief Sleep-protein synthesis integration bridge
 */
typedef struct protein_sleep_bridge_struct* protein_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-protein bridge configuration
 * WHY:  Provide sensible defaults based on sleep physiology
 * HOW:  Return config with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int protein_sleep_default_config(protein_sleep_config_t* config);

/**
 * WHAT: Create sleep-protein synthesis bridge
 * WHY:  Initialize integration between sleep and protein systems
 * HOW:  Allocate structure, link systems, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param sleep_system Sleep system handle
 * @param protein_system Protein synthesis system
 * @return New bridge or NULL on failure
 */
protein_sleep_bridge_t protein_sleep_bridge_create(
    const protein_sleep_config_t* config,
    sleep_system_t sleep_system,
    protein_synthesis_system_t protein_system
);

/**
 * WHAT: Destroy sleep-protein bridge
 * WHY:  Clean up resources and unregister callbacks
 * HOW:  Free allocations (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void protein_sleep_bridge_destroy(protein_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update protein synthesis effects from sleep system state
 * WHY:  Compute how current sleep state affects protein synthesis
 * HOW:  Query sleep state, apply modulation to synthesis rate
 *
 * @param bridge Sleep-protein bridge
 * @return 0 on success
 */
int protein_sleep_update(protein_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated protein synthesis parameters for current sleep state
 * WHY:  Apply sleep modulation to protein synthesis
 * HOW:  Return effects structure with current modulation
 *
 * @param bridge Sleep-protein bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int protein_sleep_get_effects(
    const protein_sleep_bridge_t bridge,
    protein_sleep_effects_t* effects
);

/**
 * WHAT: Get sleep-modulated synthesis rate
 * WHY:  Apply sleep state modulation to PRP synthesis
 * HOW:  Multiply base rate by sleep factor
 *
 * @param bridge Sleep-protein bridge
 * @param base_rate Base synthesis rate
 * @return Effective synthesis rate
 */
float protein_sleep_get_synthesis_rate(
    const protein_sleep_bridge_t bridge,
    float base_rate
);

/**
 * WHAT: Check if in deep sleep consolidation window
 * WHY:  Determine if optimal time for tag capture
 * HOW:  Check if current state is DEEP_NREM
 *
 * @param bridge Sleep-protein bridge
 * @return true if in deep sleep
 */
bool protein_sleep_is_consolidation_window(
    const protein_sleep_bridge_t bridge
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Get synthesis rate factor for sleep state
 * WHY:  Map sleep state to modulation factor
 * HOW:  Return predefined factor for state
 *
 * @param state Sleep state
 * @return Synthesis rate factor
 */
float protein_sleep_get_synthesis_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PROTEIN_SLEEP_BRIDGE_H */
