/**
 * @file nimcp_adaptive_sleep_bridge.h
 * @brief Sleep-Adaptive Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and adaptive threshold neurons
 * WHY:  Sleep states fundamentally alter spike threshold adaptation and sparsity
 * HOW:  Sleep state modulates threshold dynamics, adaptation rates, and sparsity targets
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → ADAPTIVE PLASTICITY PATHWAYS:
 * --------------------------------------
 * 1. Threshold Adaptation During Sleep:
 *    - AWAKE: Rapid threshold adaptation to maintain target sparsity
 *    - DROWSY: Slower adaptation (transition to consolidation mode)
 *    - LIGHT_NREM: Minimal adaptation (preserve thresholds for replay)
 *    - DEEP_NREM: Fixed thresholds (consolidation window)
 *    - REM: Moderate adaptation (exploration and creativity)
 *    - Reference: Hengen et al. (2013) "Firing rate homeostasis in visual cortex"
 *
 * 2. Sparsity Modulation:
 *    - AWAKE: Standard sparsity (70-90% inactive)
 *    - LIGHT_NREM: Increased sparsity (selective activation during replay)
 *    - DEEP_NREM: Maximum sparsity (only tagged neurons active)
 *    - REM: Reduced sparsity (increased spontaneous activity)
 *    - Reference: Peyrache et al. (2009) "Replay of rule-learning in sleep"
 *
 * 3. Soft Reset Dynamics:
 *    - AWAKE: Standard soft reset after spike
 *    - NREM: Enhanced soft reset (stronger return to baseline)
 *    - Deep NREM: Minimal reset (allow sustained activation)
 *    - REM: Standard reset
 *
 * 4. Adaptation Window:
 *    - AWAKE: Short adaptation window (responsive to input stats)
 *    - NREM: Long adaptation window (stable thresholds)
 *    - Reference: Tononi & Cirelli (2014) "Sleep function and synaptic homeostasis"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║            SLEEP-ADAPTIVE PLASTICITY INTEGRATION BRIDGE                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Adapt    Sparsity   Reset     Effect                  ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   AWAKE            1.0      0.75       1.0        Standard adaptation     ║
 * ║   DROWSY           0.5      0.80       1.1        Reduced adaptation      ║
 * ║   LIGHT_NREM       0.2      0.85       1.2        Preserve thresholds     ║
 * ║   DEEP_NREM        0.05     0.90       0.8        Fixed consolidation     ║
 * ║   REM              0.6      0.65       1.0        Exploration mode        ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ADAPTIVE_SLEEP_BRIDGE_H
#define NIMCP_ADAPTIVE_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Adaptive Modulation
 * ============================================================================ */

/* Adaptation rate modulation by sleep state */
#define ADAPTIVE_SLEEP_ADAPT_AWAKE         1.0f   /**< Full adaptation */
#define ADAPTIVE_SLEEP_ADAPT_DROWSY        0.5f   /**< Reduced adaptation */
#define ADAPTIVE_SLEEP_ADAPT_LIGHT_NREM    0.2f   /**< Minimal adaptation */
#define ADAPTIVE_SLEEP_ADAPT_DEEP_NREM     0.05f  /**< Near-frozen thresholds */
#define ADAPTIVE_SLEEP_ADAPT_REM           0.6f   /**< Moderate adaptation */

/* Sparsity target modulation by sleep state */
#define ADAPTIVE_SLEEP_SPARSITY_AWAKE      0.75f  /**< Standard sparsity */
#define ADAPTIVE_SLEEP_SPARSITY_DROWSY     0.80f  /**< Increased sparsity */
#define ADAPTIVE_SLEEP_SPARSITY_LIGHT_NREM 0.85f  /**< High sparsity */
#define ADAPTIVE_SLEEP_SPARSITY_DEEP_NREM  0.90f  /**< Maximum sparsity */
#define ADAPTIVE_SLEEP_SPARSITY_REM        0.65f  /**< Reduced sparsity */

/* Soft reset factor modulation by sleep state */
#define ADAPTIVE_SLEEP_RESET_AWAKE         1.0f   /**< Standard reset */
#define ADAPTIVE_SLEEP_RESET_DROWSY        1.1f   /**< Enhanced reset */
#define ADAPTIVE_SLEEP_RESET_LIGHT_NREM    1.2f   /**< Strong reset */
#define ADAPTIVE_SLEEP_RESET_DEEP_NREM     0.8f   /**< Weak reset (sustained) */
#define ADAPTIVE_SLEEP_RESET_REM           1.0f   /**< Standard reset */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-adaptive bridge configuration
 */
typedef struct {
    bool enable_adaptation_modulation;  /**< Enable adaptation rate changes */
    bool enable_sparsity_modulation;    /**< Enable sparsity target changes */
    bool enable_reset_modulation;       /**< Enable soft reset changes */
    float modulation_strength;          /**< Overall modulation strength (0-1) */
} adaptive_sleep_config_t;

/**
 * @brief Computed sleep effects on adaptive plasticity
 */
typedef struct {
    float adaptation_rate_factor;   /**< Multiply adaptation rate by this */
    float sparsity_target;          /**< Target sparsity for current state */
    float soft_reset_factor;        /**< Multiply reset strength by this */
    sleep_state_t current_state;    /**< Current sleep state */
    float sleep_pressure;           /**< Current sleep pressure */
    bool freeze_thresholds;         /**< True during deep NREM */
} adaptive_sleep_effects_t;

/**
 * @brief Sleep-adaptive integration bridge
 */
typedef struct adaptive_sleep_bridge_struct* adaptive_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-adaptive bridge configuration
 * WHY:  Provide sensible defaults based on firing rate homeostasis
 */
int adaptive_sleep_default_config(adaptive_sleep_config_t* config);

/**
 * WHAT: Create sleep-adaptive bridge
 * WHY:  Initialize integration between sleep and adaptive plasticity systems
 */
adaptive_sleep_bridge_t adaptive_sleep_bridge_create(
    const adaptive_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-adaptive bridge
 * WHY:  Clean up resources and unregister callbacks
 */
void adaptive_sleep_bridge_destroy(adaptive_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update adaptive effects from sleep system state
 * WHY:  Compute how current sleep state affects adaptive parameters
 */
int adaptive_sleep_update(adaptive_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated adaptive parameters for current sleep state
 * WHY:  Apply sleep modulation to threshold adaptation
 */
int adaptive_sleep_get_effects(const adaptive_sleep_bridge_t bridge,
                                adaptive_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated adaptation rate
 * WHY:  Apply adaptation rate modulation to threshold updates
 */
float adaptive_sleep_get_adaptation_rate(const adaptive_sleep_bridge_t bridge,
                                          float base_rate);

/**
 * WHAT: Get sleep-modulated sparsity target
 * WHY:  Adjust target sparsity for current sleep state
 */
float adaptive_sleep_get_sparsity_target(const adaptive_sleep_bridge_t bridge,
                                          float base_sparsity);

/**
 * WHAT: Get sleep-modulated soft reset factor
 * WHY:  Apply reset modulation to post-spike dynamics
 */
float adaptive_sleep_get_soft_reset(const adaptive_sleep_bridge_t bridge,
                                     float base_reset);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float adaptive_sleep_get_adapt_factor(sleep_state_t state);
float adaptive_sleep_get_sparsity_factor(sleep_state_t state);
float adaptive_sleep_get_reset_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ADAPTIVE_SLEEP_BRIDGE_H */
