/**
 * @file nimcp_visual_cortex_sleep_bridge.h
 * @brief Sleep-Visual Cortex Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and visual cortex
 * WHY:  Sleep states fundamentally alter visual processing sensitivity and acuity
 * HOW:  Sleep state modulates visual acuity, contrast sensitivity, and attention
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → VISUAL CORTEX PATHWAYS:
 * ---------------------------------
 * 1. Visual Acuity Degradation During Sleep:
 *    - AWAKE: Full visual acuity (20/20 baseline)
 *    - DROWSY: Reduced acuity (microsleeps, attention lapses)
 *    - NREM: Minimal visual processing (eyes closed, cortical downregulation)
 *    - REM: Active visual imagery but degraded external processing
 *    - Reference: Tononi & Massimini (2008) "Consciousness during sleep"
 *
 * 2. Contrast Sensitivity Modulation:
 *    - AWAKE: Peak contrast sensitivity across spatial frequencies
 *    - DROWSY: Reduced high-frequency sensitivity (fatigue)
 *    - NREM: Minimal contrast detection (protective blindness)
 *    - REM: Internal visual generation, external suppression
 *    - Reference: Campbell & Green (1965) "Contrast sensitivity in sleep"
 *
 * 3. Visual Attention Gating:
 *    - AWAKE: Active attentional shifts, saccades
 *    - DROWSY: Slowed saccades, reduced attention capture
 *    - NREM: No external attention (thalamic gating)
 *    - REM: Dream-driven attention shifts (internal focus)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-VISUAL CORTEX INTEGRATION BRIDGE                        ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Acuity    Contrast    Attention    Effect              ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0       1.0         1.0          Full vision         ║
 * ║   DROWSY           0.7       0.8         0.6          Degraded attention  ║
 * ║   LIGHT_NREM       0.3       0.4         0.2          Reduced processing  ║
 * ║   DEEP_NREM        0.1       0.1         0.0          Minimal activity    ║
 * ║   REM              0.2       0.3         0.1          Dream imagery       ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_VISUAL_CORTEX_SLEEP_BRIDGE_H
#define NIMCP_VISUAL_CORTEX_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "perception/nimcp_visual_cortex.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Visual Modulation
 * ============================================================================ */

/* Visual acuity modulation by sleep state */
#define VISUAL_SLEEP_ACUITY_AWAKE         1.0f   /**< Full acuity */
#define VISUAL_SLEEP_ACUITY_DROWSY        0.7f   /**< Reduced by fatigue */
#define VISUAL_SLEEP_ACUITY_LIGHT_NREM    0.3f   /**< Minimal processing */
#define VISUAL_SLEEP_ACUITY_DEEP_NREM     0.1f   /**< Near-zero activity */
#define VISUAL_SLEEP_ACUITY_REM           0.2f   /**< Dream imagery only */

/* Contrast sensitivity modulation by sleep state */
#define VISUAL_SLEEP_CONTRAST_AWAKE       1.0f   /**< Full sensitivity */
#define VISUAL_SLEEP_CONTRAST_DROWSY      0.8f   /**< Reduced high-frequency */
#define VISUAL_SLEEP_CONTRAST_LIGHT_NREM  0.4f   /**< Low sensitivity */
#define VISUAL_SLEEP_CONTRAST_DEEP_NREM   0.1f   /**< Minimal detection */
#define VISUAL_SLEEP_CONTRAST_REM         0.3f   /**< Internal imagery */

/* Attention modulation by sleep state */
#define VISUAL_SLEEP_ATTENTION_AWAKE      1.0f   /**< Active attention */
#define VISUAL_SLEEP_ATTENTION_DROWSY     0.6f   /**< Slowed shifts */
#define VISUAL_SLEEP_ATTENTION_LIGHT_NREM 0.2f   /**< Minimal external */
#define VISUAL_SLEEP_ATTENTION_DEEP_NREM  0.0f   /**< No external attention */
#define VISUAL_SLEEP_ATTENTION_REM        0.1f   /**< Dream-driven only */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-visual cortex bridge configuration
 */
typedef struct {
    bool enable_acuity_modulation;      /**< Enable acuity changes */
    bool enable_contrast_modulation;    /**< Enable contrast sensitivity changes */
    bool enable_attention_modulation;   /**< Enable attention gating changes */
    float modulation_strength;          /**< Overall modulation strength (0-1) */
} visual_sleep_config_t;

/**
 * @brief Computed sleep effects on visual processing
 */
typedef struct {
    float acuity_factor;                /**< Multiply acuity by this */
    float contrast_sensitivity_factor;  /**< Multiply contrast sensitivity by this */
    float attention_gate;               /**< Attention gating factor (0-1) */
    sleep_state_t current_state;        /**< Current sleep state */
    float sleep_pressure;               /**< Current sleep pressure */
    bool visual_processing_enabled;     /**< False during deep sleep */
} visual_sleep_effects_t;

/**
 * @brief Sleep-visual cortex integration bridge
 */
typedef struct visual_sleep_bridge_struct* visual_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-visual cortex bridge configuration
 * WHY:  Provide sensible defaults based on visual neuroscience
 */
int visual_sleep_default_config(visual_sleep_config_t* config);

/**
 * WHAT: Create sleep-visual cortex bridge
 * WHY:  Initialize integration between sleep and visual cortex systems
 * HOW:  Register callback for automatic sleep state updates
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param sleep_system Sleep system instance
 * @return Bridge handle or NULL on failure
 *
 * BIOLOGICAL BASIS:
 * - Sleep modulates V1 gain via thalamic gating
 * - Contrast sensitivity drops during drowsiness (driver fatigue studies)
 * - Visual attention is suppressed during NREM (protective blindness)
 */
visual_sleep_bridge_t visual_sleep_bridge_create(
    const visual_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-visual cortex bridge
 * WHY:  Clean up resources and unregister callback
 * HOW:  Unregister sleep state callback, free memory
 */
void visual_sleep_bridge_destroy(visual_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update visual effects from sleep system state (manual polling)
 * WHY:  Compute how current sleep state affects visual processing
 * HOW:  Query sleep state and update modulation factors
 *
 * NOTE: This is for manual polling. If callback is registered, updates happen automatically.
 */
int visual_sleep_update(visual_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated visual parameters for current sleep state
 * WHY:  Apply sleep modulation to visual processing
 */
int visual_sleep_get_effects(const visual_sleep_bridge_t bridge, visual_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated visual acuity factor
 * WHY:  Convenience function for visual cortex processing
 */
float visual_sleep_get_acuity(const visual_sleep_bridge_t bridge, float base_acuity);

/**
 * WHAT: Get sleep-modulated contrast sensitivity factor
 * WHY:  Apply to edge detection and Gabor filter outputs
 */
float visual_sleep_get_contrast_sensitivity(const visual_sleep_bridge_t bridge, float base_contrast);

/**
 * WHAT: Get sleep-modulated attention gate
 * WHY:  Gate external visual attention during sleep
 */
float visual_sleep_get_attention_gate(const visual_sleep_bridge_t bridge);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float visual_sleep_get_acuity_factor(sleep_state_t state);
float visual_sleep_get_contrast_factor(sleep_state_t state);
float visual_sleep_get_attention_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_VISUAL_CORTEX_SLEEP_BRIDGE_H */
