/**
 * @file nimcp_stdp_sleep_bridge.h
 * @brief Sleep-STDP Integration Bridge
 * @version 1.0.0
 * @date 2025-12-17
 *
 * WHAT: Bidirectional integration between sleep/wake system and STDP
 * WHY:  Sleep states fundamentally alter spike-timing dependent plasticity dynamics
 * HOW:  Sleep state modulates learning rate, LTP/LTD balance, and timing windows
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → STDP PATHWAYS:
 * -----------------------
 * 1. Synaptic Homeostasis Hypothesis (Tononi & Cirelli):
 *    - AWAKE: Net synaptic potentiation (experience-driven LTP)
 *    - NREM: Net synaptic depression (renormalization)
 *    - Sleep restores synaptic homeostasis after waking LTP
 *    - Reference: Tononi & Cirelli (2014) "Sleep and synaptic homeostasis"
 *
 * 2. STDP Window Modulation:
 *    - AWAKE: Standard asymmetric window (pre-before-post → LTP)
 *    - NREM: Wider symmetric window (replay facilitation)
 *    - REM: Enhanced LTP bias (memory consolidation)
 *    - Reference: Chauvette et al. (2012) "Sleep oscillations and STDP"
 *
 * 3. Learning Rate by Sleep State:
 *    - AWAKE: Full learning rate (online learning)
 *    - DROWSY: Reduced learning (transition)
 *    - LIGHT_NREM: Low rate, symmetric window
 *    - DEEP_NREM: LTD-biased (synaptic downscaling)
 *    - REM: LTP-biased (consolidation, integration)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    SLEEP-STDP INTEGRATION BRIDGE                           ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      LR      A+/A- Ratio   Window     Effect                ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0     1.0           Normal     Standard STDP         ║
 * ║   DROWSY           0.5     0.9           Normal     Reduced plasticity    ║
 * ║   LIGHT_NREM       0.3     0.8           Wider      Replay-friendly       ║
 * ║   DEEP_NREM        0.4     0.5 (LTD)     Widest     Synaptic downscaling  ║
 * ║   REM              0.6     1.5 (LTP)     Normal     Consolidation boost   ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_STDP_SLEEP_BRIDGE_H
#define NIMCP_STDP_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State STDP Modulation
 * ============================================================================ */

/* Learning rate modulation by sleep state */
#define STDP_SLEEP_LR_AWAKE         1.0f   /**< Full online learning */
#define STDP_SLEEP_LR_DROWSY        0.5f   /**< Reduced during transition */
#define STDP_SLEEP_LR_LIGHT_NREM    0.3f   /**< Low rate for replay */
#define STDP_SLEEP_LR_DEEP_NREM     0.4f   /**< Moderate for downscaling */
#define STDP_SLEEP_LR_REM           0.6f   /**< Enhanced for consolidation */

/* A+/A- ratio (LTP/LTD balance) by sleep state */
#define STDP_SLEEP_RATIO_AWAKE      1.0f   /**< Balanced */
#define STDP_SLEEP_RATIO_DROWSY     0.9f   /**< Slight LTD bias */
#define STDP_SLEEP_RATIO_LIGHT_NREM 0.8f   /**< LTD bias */
#define STDP_SLEEP_RATIO_DEEP_NREM  0.5f   /**< Strong LTD bias (downscaling) */
#define STDP_SLEEP_RATIO_REM        1.5f   /**< LTP bias (consolidation) */

/* Timing window modulation (multiplier for tau) */
#define STDP_SLEEP_TAU_AWAKE        1.0f   /**< Normal window */
#define STDP_SLEEP_TAU_DROWSY       1.0f   /**< Normal */
#define STDP_SLEEP_TAU_LIGHT_NREM   1.3f   /**< Wider window */
#define STDP_SLEEP_TAU_DEEP_NREM    1.5f   /**< Widest for replay */
#define STDP_SLEEP_TAU_REM          1.0f   /**< Normal window */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-STDP bridge configuration
 */
typedef struct {
    bool enable_lr_modulation;      /**< Enable learning rate changes */
    bool enable_ratio_modulation;   /**< Enable LTP/LTD ratio changes */
    bool enable_window_modulation;  /**< Enable timing window changes */
    float modulation_strength;      /**< Overall modulation strength (0-1) */
} stdp_sleep_config_t;

/**
 * @brief Computed sleep effects on STDP
 */
typedef struct {
    float learning_rate_factor;     /**< Multiply base LR by this */
    float ltp_ltd_ratio;            /**< A+/A- ratio modifier */
    float tau_factor;               /**< Timing window multiplier */
    sleep_state_t current_state;    /**< Current sleep state */
    float sleep_pressure;           /**< Current sleep pressure */
    bool plasticity_enabled;        /**< False during deep sleep offline */
} stdp_sleep_effects_t;

/**
 * @brief Sleep-STDP integration bridge
 */
typedef struct stdp_sleep_bridge_struct* stdp_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-STDP bridge configuration
 * WHY:  Provide sensible defaults based on synaptic homeostasis hypothesis
 */
int stdp_sleep_default_config(stdp_sleep_config_t* config);

/**
 * WHAT: Create sleep-STDP bridge
 * WHY:  Initialize integration between sleep and STDP systems
 */
stdp_sleep_bridge_t stdp_sleep_bridge_create(
    const stdp_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-STDP bridge
 * WHY:  Clean up resources
 */
void stdp_sleep_bridge_destroy(stdp_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update STDP effects from sleep system state
 * WHY:  Compute how current sleep state affects STDP parameters
 */
int stdp_sleep_update(stdp_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated STDP parameters for current sleep state
 * WHY:  Apply sleep modulation to synapse updates
 */
int stdp_sleep_get_effects(const stdp_sleep_bridge_t bridge, stdp_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated learning rate
 * WHY:  Convenience function for STDP update calls
 */
float stdp_sleep_get_learning_rate(const stdp_sleep_bridge_t bridge, float base_lr);

/**
 * WHAT: Get sleep-modulated A+ amplitude
 * WHY:  Apply LTP/LTD ratio to A+ for sleep state
 */
float stdp_sleep_get_a_plus(const stdp_sleep_bridge_t bridge, float base_a_plus);

/**
 * WHAT: Get sleep-modulated A- amplitude
 * WHY:  Apply LTP/LTD ratio to A- for sleep state
 */
float stdp_sleep_get_a_minus(const stdp_sleep_bridge_t bridge, float base_a_minus);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

float stdp_sleep_get_lr_factor(sleep_state_t state);
float stdp_sleep_get_ratio_factor(sleep_state_t state);
float stdp_sleep_get_tau_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STDP_SLEEP_BRIDGE_H */
