/**
 * @file nimcp_bcm_sleep_bridge.h
 * @brief Sleep-BCM Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and BCM plasticity
 * WHY:  Sleep states alter the sliding threshold dynamics of BCM learning
 * HOW:  Sleep state modulates BCM threshold (theta_m) and learning rate
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * BCM THEORY AND SLEEP:
 * ----------------------
 * The BCM rule: dw = eta * post * (post - theta_m) * pre
 * - theta_m is a sliding threshold that determines LTP vs LTD boundary
 * - Sleep modulates theta_m to enable different plasticity regimes
 *
 * SLEEP → BCM PATHWAYS:
 * ----------------------
 * 1. Threshold Elevation During NREM:
 *    - Higher theta_m → more activity needed for LTP
 *    - Net effect: favors LTD (synaptic downscaling)
 *    - Implements Tononi's synaptic homeostasis hypothesis
 *
 * 2. Threshold Depression During REM:
 *    - Lower theta_m → easier to achieve LTP
 *    - Net effect: favors consolidation of important memories
 *
 * 3. Sleep Pressure Effects:
 *    - High adenosine → elevated theta_m
 *    - Models metabolic constraint on plasticity
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                     SLEEP-BCM INTEGRATION BRIDGE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      theta_m    LR Factor   Effect                          ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0x       1.0         Normal BCM dynamics             ║
 * ║   DROWSY           1.1x       0.6         Slightly elevated threshold     ║
 * ║   LIGHT_NREM       1.3x       0.4         Elevated → more LTD             ║
 * ║   DEEP_NREM        1.5x       0.5         Maximum elevation (downscaling) ║
 * ║   REM              0.7x       0.7         Lowered → favors LTP            ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BCM_SLEEP_BRIDGE_H
#define NIMCP_BCM_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Theta_m modulation by sleep state (multiplier) */
#define BCM_SLEEP_THETA_AWAKE       1.0f   /**< Normal threshold */
#define BCM_SLEEP_THETA_DROWSY      1.1f   /**< Slightly elevated */
#define BCM_SLEEP_THETA_LIGHT_NREM  1.3f   /**< Elevated for LTD */
#define BCM_SLEEP_THETA_DEEP_NREM   1.5f   /**< Maximum (downscaling) */
#define BCM_SLEEP_THETA_REM         0.7f   /**< Lowered for LTP */

/* Learning rate modulation */
#define BCM_SLEEP_LR_AWAKE          1.0f
#define BCM_SLEEP_LR_DROWSY         0.6f
#define BCM_SLEEP_LR_LIGHT_NREM     0.4f
#define BCM_SLEEP_LR_DEEP_NREM      0.5f
#define BCM_SLEEP_LR_REM            0.7f

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

typedef struct {
    bool enable_theta_modulation;
    bool enable_lr_modulation;
    float modulation_strength;
} bcm_sleep_config_t;

typedef struct {
    float theta_factor;             /**< Multiply theta_m by this */
    float learning_rate_factor;     /**< Multiply LR by this */
    sleep_state_t current_state;
    float sleep_pressure;
    bool favors_ltd;                /**< True if current state favors LTD */
} bcm_sleep_effects_t;

typedef struct bcm_sleep_bridge_struct* bcm_sleep_bridge_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

int bcm_sleep_default_config(bcm_sleep_config_t* config);

bcm_sleep_bridge_t bcm_sleep_bridge_create(
    const bcm_sleep_config_t* config,
    sleep_system_t sleep_system
);

void bcm_sleep_bridge_destroy(bcm_sleep_bridge_t bridge);

int bcm_sleep_update(bcm_sleep_bridge_t bridge);

int bcm_sleep_get_effects(const bcm_sleep_bridge_t bridge, bcm_sleep_effects_t* effects);

float bcm_sleep_get_theta_factor(const bcm_sleep_bridge_t bridge);

float bcm_sleep_get_lr_factor(const bcm_sleep_bridge_t bridge);

/* Helper functions */
float bcm_sleep_theta_for_state(sleep_state_t state);
float bcm_sleep_lr_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BCM_SLEEP_BRIDGE_H */
