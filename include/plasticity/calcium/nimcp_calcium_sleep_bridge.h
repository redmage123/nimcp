/**
 * @file nimcp_calcium_sleep_bridge.h
 * @brief Sleep-Calcium Dynamics Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between sleep/wake system and calcium dynamics
 * WHY:  Sleep states fundamentally alter calcium signaling and NMDA activity
 * HOW:  Sleep state modulates calcium influx, decay, and learning rate computation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → CALCIUM PATHWAYS:
 * -------------------------
 * 1. NMDA Receptor Activity During Sleep:
 *    - AWAKE: Standard NMDA activity (input-driven)
 *    - DROWSY: Reduced NMDA (transition to offline processing)
 *    - LIGHT_NREM: Minimal NMDA (upstate/downstate oscillations)
 *    - DEEP_NREM: Very low NMDA (downstate dominates, synaptic consolidation)
 *    - REM: Moderate NMDA (replay-driven, theta rhythm)
 *    - Reference: Chauvette et al. (2012) "Sleep oscillations in the thalamocortical system"
 *
 * 2. Calcium Influx Modulation:
 *    - AWAKE: 100% influx (full NMDA activation)
 *    - DROWSY: 70% influx (reduced sensory input)
 *    - LIGHT_NREM: 40% influx (spontaneous upstates)
 *    - DEEP_NREM: 20% influx (minimal activity, consolidation mode)
 *    - REM: 60% influx (replay-driven reactivation)
 *    - Reference: Miyamoto et al. (2016) "Top-down cortical input during NREM sleep"
 *
 * 3. Calcium Decay and Homeostasis:
 *    - AWAKE: Standard decay (τ = 50 ms)
 *    - NREM: Enhanced decay (τ = 30 ms, faster clearance)
 *    - Deep NREM: Maximum clearance (restore baseline for consolidation)
 *    - REM: Standard decay
 *    - Reference: Ding et al. (2016) "Changes in synaptic plasticity during sleep"
 *
 * 4. Learning Rate Modulation:
 *    - AWAKE: Full learning (standard omega function)
 *    - NREM: Reduced learning (consolidation, not acquisition)
 *    - Deep NREM: Minimal learning (preservation mode)
 *    - REM: Moderate learning (pattern integration)
 *    - Reference: Tononi & Cirelli (2014) "Sleep and synaptic homeostasis"
 *
 * 5. Calcium Threshold Shifts:
 *    - AWAKE: Standard thresholds (θ_LTD=0.35, θ_LTP=0.55)
 *    - Deep NREM: Higher thresholds (favor stability)
 *    - REM: Lower thresholds (favor plasticity)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-CALCIUM DYNAMICS INTEGRATION BRIDGE                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      NMDA      Ca²⁺       Decay    Learning   Effect        ║
 * ║                    Influx    Clearance  τ (ms)   Rate                     ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   AWAKE            100%      Standard   50       100%      Full learning  ║
 * ║   DROWSY           70%       Standard   45       70%       Reduced        ║
 * ║   LIGHT_NREM       40%       Enhanced   35       40%       Consolidation  ║
 * ║   DEEP_NREM        20%       Maximum    25       20%       Preservation   ║
 * ║   REM              60%       Standard   50       60%       Integration    ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CALCIUM_SLEEP_BRIDGE_H
#define NIMCP_CALCIUM_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/calcium/nimcp_calcium_dynamics.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Calcium Modulation
 * ============================================================================ */

/* NMDA influx modulation by sleep state */
#define CALCIUM_SLEEP_INFLUX_AWAKE         1.0f   /**< Full NMDA activity */
#define CALCIUM_SLEEP_INFLUX_DROWSY        0.7f   /**< Reduced NMDA */
#define CALCIUM_SLEEP_INFLUX_LIGHT_NREM    0.4f   /**< Minimal NMDA (upstates) */
#define CALCIUM_SLEEP_INFLUX_DEEP_NREM     0.2f   /**< Very low NMDA */
#define CALCIUM_SLEEP_INFLUX_REM           0.6f   /**< Moderate NMDA (replay) */

/* Calcium decay time constant modulation (ms) */
#define CALCIUM_SLEEP_DECAY_AWAKE          50.0f  /**< Standard decay */
#define CALCIUM_SLEEP_DECAY_DROWSY         45.0f  /**< Slightly faster */
#define CALCIUM_SLEEP_DECAY_LIGHT_NREM     35.0f  /**< Enhanced clearance */
#define CALCIUM_SLEEP_DECAY_DEEP_NREM      25.0f  /**< Maximum clearance */
#define CALCIUM_SLEEP_DECAY_REM            50.0f  /**< Standard decay */

/* Learning rate modulation by sleep state */
#define CALCIUM_SLEEP_LR_AWAKE             1.0f   /**< Full learning */
#define CALCIUM_SLEEP_LR_DROWSY            0.7f   /**< Reduced learning */
#define CALCIUM_SLEEP_LR_LIGHT_NREM        0.4f   /**< Consolidation mode */
#define CALCIUM_SLEEP_LR_DEEP_NREM         0.2f   /**< Preservation mode */
#define CALCIUM_SLEEP_LR_REM               0.6f   /**< Integration mode */

/* Pump rate modulation by sleep state */
#define CALCIUM_SLEEP_PUMP_AWAKE           1.0f   /**< Standard extrusion */
#define CALCIUM_SLEEP_PUMP_DROWSY          1.2f   /**< Enhanced extrusion */
#define CALCIUM_SLEEP_PUMP_LIGHT_NREM      1.5f   /**< Stronger extrusion */
#define CALCIUM_SLEEP_PUMP_DEEP_NREM       2.0f   /**< Maximum extrusion */
#define CALCIUM_SLEEP_PUMP_REM             1.0f   /**< Standard extrusion */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-calcium bridge configuration
 */
typedef struct {
    bool enable_influx_modulation;      /**< Enable NMDA influx changes */
    bool enable_decay_modulation;       /**< Enable decay time constant changes */
    bool enable_lr_modulation;          /**< Enable learning rate scaling */
    bool enable_pump_modulation;        /**< Enable pump rate changes */
    bool enable_threshold_shifts;       /**< Enable threshold shifts by state */
    float modulation_strength;          /**< Overall modulation strength (0-1) */
} calcium_sleep_config_t;

/**
 * @brief Computed sleep effects on calcium dynamics
 */
typedef struct {
    float influx_factor;                /**< Multiply NMDA influx by this */
    float decay_tau_ms;                 /**< Modulated decay time constant */
    float learning_rate_factor;         /**< Multiply learning rate by this */
    float pump_rate_factor;             /**< Multiply pump rate by this */
    float threshold_ltd_shift;          /**< Shift to θ_LTD (μM) */
    float threshold_ltp_shift;          /**< Shift to θ_LTP (μM) */
    sleep_state_t current_state;        /**< Current sleep state */
    float sleep_pressure;               /**< Current sleep pressure */
} calcium_sleep_effects_t;

/**
 * @brief Sleep-calcium integration bridge
 */
typedef struct calcium_sleep_bridge_struct* calcium_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-calcium bridge configuration
 * WHY:  Provide sensible defaults based on sleep neuroscience
 * HOW:  Return struct with evidence-based parameters
 */
int calcium_sleep_default_config(calcium_sleep_config_t* config);

/**
 * WHAT: Create sleep-calcium bridge
 * WHY:  Initialize integration between sleep and calcium systems
 * HOW:  Allocate structure, register sleep state callback
 */
calcium_sleep_bridge_t calcium_sleep_bridge_create(
    const calcium_sleep_config_t* config,
    sleep_system_t sleep_system,
    calcium_dynamics_t calcium
);

/**
 * WHAT: Destroy sleep-calcium bridge
 * WHY:  Clean up resources and unregister callbacks
 * HOW:  Free structure, unregister from sleep system
 */
void calcium_sleep_bridge_destroy(calcium_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * WHAT: Update calcium effects from sleep system state
 * WHY:  Compute how current sleep state affects calcium dynamics
 * HOW:  Query sleep state, compute modulation factors
 */
int calcium_sleep_update(calcium_sleep_bridge_t bridge);

/**
 * WHAT: Get modulated calcium parameters for current sleep state
 * WHY:  Apply sleep modulation to calcium dynamics
 * HOW:  Return computed effects structure
 */
int calcium_sleep_get_effects(const calcium_sleep_bridge_t bridge,
                               calcium_sleep_effects_t* effects);

/**
 * WHAT: Get sleep-modulated NMDA influx factor
 * WHY:  Apply influx modulation to calcium entry
 * HOW:  Return influx multiplier for current sleep state
 */
float calcium_sleep_get_influx_factor(const calcium_sleep_bridge_t bridge);

/**
 * WHAT: Get sleep-modulated decay time constant
 * WHY:  Adjust calcium clearance rate for sleep state
 * HOW:  Return decay τ for current state
 */
float calcium_sleep_get_decay_tau(const calcium_sleep_bridge_t bridge);

/**
 * WHAT: Get sleep-modulated learning rate
 * WHY:  Scale omega function output by sleep state
 * HOW:  Apply learning rate factor to base LR
 */
float calcium_sleep_get_learning_rate(const calcium_sleep_bridge_t bridge,
                                       float base_lr);

/**
 * WHAT: Apply sleep modulation to calcium system
 * WHY:  Update calcium dynamics parameters for current sleep state
 * HOW:  Modify calcium config based on computed effects
 */
int calcium_sleep_apply_modulation(calcium_sleep_bridge_t bridge);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get NMDA influx factor for sleep state
 */
float calcium_sleep_get_influx_factor_for_state(sleep_state_t state);

/**
 * @brief Get decay time constant for sleep state
 */
float calcium_sleep_get_decay_tau_for_state(sleep_state_t state);

/**
 * @brief Get learning rate factor for sleep state
 */
float calcium_sleep_get_lr_factor_for_state(sleep_state_t state);

/**
 * @brief Get pump rate factor for sleep state
 */
float calcium_sleep_get_pump_factor_for_state(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CALCIUM_SLEEP_BRIDGE_H */
