/**
 * @file nimcp_homeostatic_sleep_bridge.h
 * @brief Sleep-Homeostatic Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and homeostatic plasticity
 * WHY:  Sleep is the PRIMARY time for homeostatic synaptic regulation (Tononi SHY)
 * HOW:  Sleep state controls when and how much synaptic scaling occurs
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SYNAPTIC HOMEOSTASIS HYPOTHESIS (SHY) - Tononi & Cirelli:
 * ----------------------------------------------------------
 * Core claim: Sleep is necessary for synaptic homeostasis
 *
 * 1. During WAKE:
 *    - Learning causes net synaptic potentiation
 *    - Synapses grow stronger on average
 *    - Energy and space become limiting factors
 *    - Homeostatic scaling is SUPPRESSED
 *
 * 2. During NREM SLEEP:
 *    - Global synaptic downscaling occurs
 *    - All synapses reduced proportionally (~20% in deep NREM)
 *    - Weak synapses eliminated, strong preserved (SNR improves)
 *    - Homeostatic scaling is MAXIMALLY ACTIVE
 *
 * 3. During REM SLEEP:
 *    - Scaling paused for consolidation
 *    - Selective strengthening of important patterns
 *    - Homeostatic scaling REDUCED
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-HOMEOSTATIC PLASTICITY INTEGRATION                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Scaling Rate   Target Rate   Pruning    Effect         ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            0.0            Unchanged     Off        Scaling OFF    ║
 * ║   DROWSY           0.2            Normal        Low        Beginning      ║
 * ║   LIGHT_NREM       0.6            -10%          Medium     Active         ║
 * ║   DEEP_NREM        1.0 (MAX)      -20%          High       Full downscale ║
 * ║   REM              0.2            Normal        Low        Consolidation  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_HOMEOSTATIC_SLEEP_BRIDGE_H
#define NIMCP_HOMEOSTATIC_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Scaling rate by sleep state (0 = no scaling, 1 = maximum) */
#define HOMEO_SLEEP_SCALE_RATE_AWAKE       0.0f   /**< No scaling during wake */
#define HOMEO_SLEEP_SCALE_RATE_DROWSY      0.2f   /**< Beginning scaling */
#define HOMEO_SLEEP_SCALE_RATE_LIGHT_NREM  0.6f   /**< Active scaling */
#define HOMEO_SLEEP_SCALE_RATE_DEEP_NREM   1.0f   /**< Maximum scaling (SHY) */
#define HOMEO_SLEEP_SCALE_RATE_REM         0.2f   /**< Reduced for consolidation */

/* Target rate adjustment (relative change) */
#define HOMEO_SLEEP_TARGET_AWAKE           1.0f   /**< Maintain normal target */
#define HOMEO_SLEEP_TARGET_DROWSY          1.0f   /**< Normal */
#define HOMEO_SLEEP_TARGET_LIGHT_NREM      0.9f   /**< 10% lower target */
#define HOMEO_SLEEP_TARGET_DEEP_NREM       0.8f   /**< 20% lower (downscaling) */
#define HOMEO_SLEEP_TARGET_REM             1.0f   /**< Normal */

/* Pruning threshold modifier */
#define HOMEO_SLEEP_PRUNE_AWAKE            0.0f   /**< No pruning */
#define HOMEO_SLEEP_PRUNE_DROWSY           0.3f   /**< Light pruning */
#define HOMEO_SLEEP_PRUNE_LIGHT_NREM       0.6f   /**< Moderate pruning */
#define HOMEO_SLEEP_PRUNE_DEEP_NREM        1.0f   /**< Maximum pruning */
#define HOMEO_SLEEP_PRUNE_REM              0.3f   /**< Light pruning */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

typedef struct {
    bool enable_scaling_modulation;
    bool enable_target_modulation;
    bool enable_pruning_modulation;
    float modulation_strength;
    float deep_nrem_scaling_boost;  /**< Extra boost during deep NREM */
} homeostatic_sleep_config_t;

typedef struct {
    float scaling_rate_factor;      /**< Current scaling rate (0-1) */
    float target_rate_modifier;     /**< Target rate adjustment */
    float pruning_threshold_mod;    /**< Pruning aggressiveness */
    sleep_state_t current_state;
    float sleep_pressure;
    bool scaling_active;            /**< True if any scaling occurring */
    bool is_deep_nrem;              /**< Special flag for max scaling */
} homeostatic_sleep_effects_t;

typedef struct homeostatic_sleep_bridge_struct* homeostatic_sleep_bridge_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

int homeostatic_sleep_default_config(homeostatic_sleep_config_t* config);

homeostatic_sleep_bridge_t homeostatic_sleep_bridge_create(
    const homeostatic_sleep_config_t* config,
    sleep_system_t sleep_system
);

void homeostatic_sleep_bridge_destroy(homeostatic_sleep_bridge_t bridge);

int homeostatic_sleep_update(homeostatic_sleep_bridge_t bridge);

int homeostatic_sleep_get_effects(
    const homeostatic_sleep_bridge_t bridge,
    homeostatic_sleep_effects_t* effects
);

float homeostatic_sleep_get_scaling_rate(const homeostatic_sleep_bridge_t bridge);

bool homeostatic_sleep_is_scaling_active(const homeostatic_sleep_bridge_t bridge);

/* Helper functions */
float homeostatic_sleep_scaling_for_state(sleep_state_t state);
float homeostatic_sleep_target_for_state(sleep_state_t state);
float homeostatic_sleep_pruning_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HOMEOSTATIC_SLEEP_BRIDGE_H */
