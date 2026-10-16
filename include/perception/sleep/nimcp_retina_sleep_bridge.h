/**
 * @file nimcp_retina_sleep_bridge.h
 * @brief Sleep-Retina Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and retina
 * WHY:  Sleep states fundamentally alter retinal sensitivity and pupil response
 * HOW:  Sleep state modulates pupil response, light sensitivity, photoreceptor adaptation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → RETINA PATHWAYS:
 * -------------------------
 * 1. Pupil Response Modulation:
 *    - AWAKE: Active pupillary light reflex (2-8mm diameter, 300ms latency)
 *    - DROWSY: Slowed pupil response (fatigue, microsleeps)
 *    - NREM: Miosis (constricted pupils ~2-3mm, reduced reflex)
 *    - REM: Rapid eye movements but reduced pupil reflex
 *    - Reference: Yoss et al. (1970) "Pupil size and drowsiness"
 *
 * 2. Light Sensitivity Changes:
 *    - AWAKE: Normal photopic/scotopic adaptation
 *    - DROWSY: Reduced sensitivity (delayed dark adaptation)
 *    - NREM: Eyes closed, minimal retinal activity
 *    - REM: Eyelids closed but internal visual imagery (PGO waves)
 *    - Reference: Schmidt et al. (2009) "Light sensitivity during sleep"
 *
 * 3. Photoreceptor Adaptation:
 *    - AWAKE: Active light/dark adaptation (~30min for full dark adaptation)
 *    - DROWSY: Slower adaptation kinetics
 *    - NREM: Retinal recovery from phototoxicity (dark rest)
 *    - REM: Rapid eye movements generate retinal signals (saccadic suppression)
 *    - Reference: Hannibal et al. (2002) "Melanopsin in sleep-wake regulation"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  SLEEP-RETINA INTEGRATION BRIDGE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Pupil     Light      Adaptation  Effect                ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0       1.0        1.0         Normal vision         ║
 * ║   DROWSY           0.7       0.8        0.6         Slowed response       ║
 * ║   LIGHT_NREM       0.3       0.4        0.3         Eyes closing          ║
 * ║   DEEP_NREM        0.1       0.1        0.2         Miosis, recovery      ║
 * ║   REM              0.2       0.2        0.4         REMs but closed lids  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_RETINA_SLEEP_BRIDGE_H
#define NIMCP_RETINA_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "perception/nimcp_retina.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Retinal Modulation
 * ============================================================================ */

/* Pupil response modulation by sleep state */
#define RETINA_SLEEP_PUPIL_AWAKE         1.0f   /**< Active pupillary reflex */
#define RETINA_SLEEP_PUPIL_DROWSY        0.7f   /**< Slowed response */
#define RETINA_SLEEP_PUPIL_LIGHT_NREM    0.3f   /**< Reduced reflex */
#define RETINA_SLEEP_PUPIL_DEEP_NREM     0.1f   /**< Miosis, minimal reflex */
#define RETINA_SLEEP_PUPIL_REM           0.2f   /**< REMs but reduced reflex */

/* Light sensitivity modulation by sleep state */
#define RETINA_SLEEP_SENSITIVITY_AWAKE      1.0f   /**< Normal sensitivity */
#define RETINA_SLEEP_SENSITIVITY_DROWSY     0.8f   /**< Slightly reduced */
#define RETINA_SLEEP_SENSITIVITY_LIGHT_NREM 0.4f   /**< Eyes closing */
#define RETINA_SLEEP_SENSITIVITY_DEEP_NREM  0.1f   /**< Eyes closed */
#define RETINA_SLEEP_SENSITIVITY_REM        0.2f   /**< Closed but active */

/* Photoreceptor adaptation modulation by sleep state */
#define RETINA_SLEEP_ADAPTATION_AWAKE       1.0f   /**< Normal adaptation */
#define RETINA_SLEEP_ADAPTATION_DROWSY      0.6f   /**< Slower kinetics */
#define RETINA_SLEEP_ADAPTATION_LIGHT_NREM  0.3f   /**< Reduced adaptation */
#define RETINA_SLEEP_ADAPTATION_DEEP_NREM   0.2f   /**< Recovery mode */
#define RETINA_SLEEP_ADAPTATION_REM         0.4f   /**< REM recovery */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-retina bridge configuration
 */
typedef struct {
    bool enable_pupil_modulation;        /**< Enable pupil response changes */
    bool enable_sensitivity_modulation;  /**< Enable light sensitivity changes */
    bool enable_adaptation_modulation;   /**< Enable adaptation rate changes */
    float modulation_strength;           /**< Overall modulation strength (0-1) */
} retina_sleep_config_t;

/**
 * @brief Computed sleep effects on retinal processing
 */
typedef struct {
    float pupil_response_factor;         /**< Multiply pupil response by this */
    float light_sensitivity_factor;      /**< Multiply light sensitivity by this */
    float adaptation_rate_factor;        /**< Multiply adaptation rate by this */
    sleep_state_t current_state;         /**< Current sleep state */
    float sleep_pressure;                /**< Current sleep pressure */
    bool retinal_processing_enabled;     /**< False when eyes closed */
} retina_sleep_effects_t;

/**
 * @brief Sleep-retina integration bridge
 */
typedef struct retina_sleep_bridge_struct* retina_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-retina bridge configuration
 * WHY:  Provide sensible defaults based on retinal physiology
 */
int retina_sleep_default_config(retina_sleep_config_t* config);

/**
 * WHAT: Create sleep-retina bridge
 * WHY:  Initialize integration between sleep and retina systems
 * HOW:  Register callback for automatic sleep state updates
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param sleep_system Sleep system instance
 * @return Bridge handle or NULL on failure
 *
 * BIOLOGICAL BASIS:
 * - Pupil diameter decreases during NREM sleep (miosis)
 * - Light sensitivity reduces when eyes close during sleep
 * - Retina recovers from phototoxicity during dark rest (sleep)
 */
retina_sleep_bridge_t retina_sleep_bridge_create(
    const retina_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-retina bridge
 * WHY:  Clean up resources and unregister callback
 * HOW:  Unregister sleep state callback, free memory
 */
void retina_sleep_bridge_destroy(retina_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update retinal effects from sleep system state (manual polling)
 * WHY:  Compute how current sleep state affects retinal processing
 * HOW:  Query sleep state and update modulation factors
 *
 * NOTE: This is for manual polling. If callback is registered, updates happen automatically.
 */
int retina_sleep_update(retina_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated retinal parameters for current sleep state
 * WHY:  Apply sleep modulation to retinal processing
 */
int retina_sleep_get_effects(const retina_sleep_bridge_t bridge, retina_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated pupil response factor
 * WHY:  Convenience function for pupil light reflex
 */
float retina_sleep_get_pupil_response(const retina_sleep_bridge_t bridge, float base_response);

/**
 * WHAT: Get sleep-modulated light sensitivity factor
 * WHY:  Apply to photoreceptor gain
 */
float retina_sleep_get_light_sensitivity(const retina_sleep_bridge_t bridge, float base_sensitivity);

/**
 * WHAT: Get sleep-modulated adaptation rate factor
 * WHY:  Adjust light/dark adaptation kinetics
 */
float retina_sleep_get_adaptation_rate(const retina_sleep_bridge_t bridge, float base_rate);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float retina_sleep_get_pupil_factor(sleep_state_t state);
float retina_sleep_get_sensitivity_factor(sleep_state_t state);
float retina_sleep_get_adaptation_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_RETINA_SLEEP_BRIDGE_H */
