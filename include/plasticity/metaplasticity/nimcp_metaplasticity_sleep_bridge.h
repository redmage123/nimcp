/**
 * @file nimcp_metaplasticity_sleep_bridge.h
 * @brief Sleep-Metaplasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between sleep/wake system and extended metaplasticity
 * WHY:  Sleep fundamentally resets plasticity thresholds via synaptic homeostasis
 * HOW:  Sleep state triggers threshold reset toward baseline, modulates adaptation rates
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → METAPLASTICITY PATHWAYS:
 * ---------------------------------
 * 1. Synaptic Homeostasis Hypothesis (Tononi & Cirelli 2014):
 *    - AWAKE: Thresholds drift upward from LTP accumulation
 *    - NREM Sleep: Thresholds reset toward baseline (renormalization)
 *    - Deep NREM: Strong reset (70-90% back to baseline)
 *    - REM: Moderate reset with consolidation (40-60%)
 *    - Reference: "Sleep and synaptic homeostasis: a hypothesis"
 *
 * 2. Threshold Reset Dynamics:
 *    - Light NREM (Stage 2): 40% reset toward baseline
 *    - Deep NREM (SWS): 80% reset toward baseline
 *    - REM: 50% reset with selective preservation
 *    - θ_m(wake) = θ_m(sleep) × (1-α) + θ_baseline × α
 *
 * 3. Adaptation Rate Modulation:
 *    - AWAKE: Fast threshold adaptation (track activity)
 *    - NREM: Frozen adaptation (preserve consolidation window)
 *    - Deep NREM: Complete freeze (no new learning)
 *    - REM: Moderate adaptation (integrate memories)
 *
 * 4. Sleep Pressure Effects:
 *    - High sleep pressure → stronger threshold reset
 *    - Extended wake → accumulated threshold drift
 *    - Sleep deprivation → impaired metaplasticity
 *
 * METAPLASTICITY → SLEEP PATHWAYS:
 * ---------------------------------
 * 1. Threshold Drift Monitoring:
 *    - Excessive θ_m drift → increased sleep pressure
 *    - Threshold instability → trigger NREM need
 *    - Supports homeostatic sleep regulation
 *
 * 2. Reset Completion Feedback:
 *    - Successful threshold reset → sleep quality metric
 *    - Incomplete reset → extend NREM duration
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                SLEEP-METAPLASTICITY INTEGRATION BRIDGE                     ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Reset     Adapt    Effect                              ║
 * ║   ────────────────────────────────────────────────────────────────────    ║
 * ║   AWAKE            0%        100%     Normal threshold tracking            ║
 * ║   DROWSY           10%       50%      Transition to consolidation          ║
 * ║   LIGHT_NREM       40%       20%      Moderate renormalization             ║
 * ║   DEEP_NREM        80%       5%       Strong homeostatic reset             ║
 * ║   REM              50%       60%      Selective consolidation              ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 * @reference Tononi & Cirelli (2014) "Sleep and synaptic homeostasis"
 */

#ifndef NIMCP_METAPLASTICITY_SLEEP_BRIDGE_H
#define NIMCP_METAPLASTICITY_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/metaplasticity/nimcp_extended_metaplasticity.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Metaplasticity Modulation
 * ============================================================================ */

/* Threshold reset factors by sleep state (same as main module) */
#define METAPLASTICITY_SLEEP_BRIDGE_RESET_AWAKE       0.0f   /**< No reset */
#define METAPLASTICITY_SLEEP_BRIDGE_RESET_DROWSY      0.1f   /**< 10% reset */
#define METAPLASTICITY_SLEEP_BRIDGE_RESET_LIGHT_NREM  0.4f   /**< 40% reset */
#define METAPLASTICITY_SLEEP_BRIDGE_RESET_DEEP_NREM   0.8f   /**< 80% reset */
#define METAPLASTICITY_SLEEP_BRIDGE_RESET_REM         0.5f   /**< 50% reset */

/* Adaptation rate modulation by sleep state */
#define METAPLASTICITY_SLEEP_ADAPT_AWAKE              1.0f   /**< Full adaptation */
#define METAPLASTICITY_SLEEP_ADAPT_DROWSY             0.5f   /**< Reduced */
#define METAPLASTICITY_SLEEP_ADAPT_LIGHT_NREM         0.2f   /**< Minimal */
#define METAPLASTICITY_SLEEP_ADAPT_DEEP_NREM          0.05f  /**< Nearly frozen */
#define METAPLASTICITY_SLEEP_ADAPT_REM                0.6f   /**< Moderate */

/* Threshold drift tolerance (when to signal sleep need) */
#define METAPLASTICITY_THRESHOLD_DRIFT_TOLERANCE      0.5f   /**< 50% drift from baseline */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-metaplasticity bridge configuration
 */
typedef struct {
    bool enable_threshold_reset;        /**< Enable sleep-dependent reset */
    bool enable_adaptation_freeze;      /**< Enable adaptation rate modulation */
    bool enable_drift_monitoring;       /**< Monitor threshold drift */
    bool enable_sleep_pressure_feedback; /**< Signal sleep need from drift */
    float reset_strength_multiplier;    /**< Overall reset strength [0-2] */
    float adaptation_strength_multiplier; /**< Overall adaptation modulation [0-2] */
} metaplasticity_sleep_config_t;

/**
 * @brief Computed sleep effects on metaplasticity
 */
typedef struct {
    float threshold_reset_factor;       /**< Current reset strength [0-1] */
    float adaptation_rate_factor;       /**< Multiply adaptation by this */
    float baseline_drift;               /**< Current drift from baseline */
    sleep_state_t current_state;        /**< Current sleep state */
    float sleep_pressure_contribution;  /**< Contribution to sleep pressure */
    bool adaptation_frozen;             /**< True during deep NREM */
} metaplasticity_sleep_effects_t;

/**
 * @brief Sleep-metaplasticity integration bridge
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    sleep_system_t sleep_system;
    metaplasticity_controller_t metaplasticity_controller;

    /* Configuration */
    metaplasticity_sleep_config_t config;

    /* Current effects */
    metaplasticity_sleep_effects_t effects;

    /* Statistics */
    uint64_t total_resets;
    uint64_t deep_nrem_resets;
    uint64_t rem_resets;
    float total_drift_accumulated;
    float max_drift_observed;

    } metaplasticity_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default sleep-metaplasticity bridge configuration
 *
 * WHAT: Provide sensible defaults for sleep-metaplasticity integration
 * WHY:  Based on synaptic homeostasis hypothesis
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int metaplasticity_sleep_default_config(metaplasticity_sleep_config_t* config);

/**
 * @brief Create sleep-metaplasticity bridge
 *
 * WHAT: Initialize bidirectional sleep-metaplasticity integration
 * WHY:  Enable sleep-dependent threshold homeostasis
 * HOW:  Allocate structure, link subsystems, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param sleep_system Sleep/wake system
 * @param metaplasticity_controller Metaplasticity controller
 * @return New bridge or NULL on failure
 */
metaplasticity_sleep_bridge_t* metaplasticity_sleep_bridge_create(
    const metaplasticity_sleep_config_t* config,
    sleep_system_t sleep_system,
    metaplasticity_controller_t metaplasticity_controller
);

/**
 * @brief Destroy sleep-metaplasticity bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister callbacks, free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void metaplasticity_sleep_bridge_destroy(metaplasticity_sleep_bridge_t* bridge);

/* ============================================================================
 * Sleep → Metaplasticity API
 * ============================================================================ */

/**
 * @brief Apply sleep-dependent threshold reset
 *
 * WHAT: Reset plasticity thresholds toward baseline during sleep
 * WHY:  Implement synaptic homeostasis hypothesis
 * HOW:  θ_m ← θ_m × (1 - reset_factor) + θ_baseline × reset_factor
 *
 * BIOLOGICAL: Tononi & Cirelli (2014) - NREM sleep renormalizes synapses
 *
 * @param bridge Sleep-metaplasticity bridge
 * @return 0 on success, -1 on error
 */
int metaplasticity_sleep_apply_threshold_reset(metaplasticity_sleep_bridge_t* bridge);

/**
 * @brief Freeze threshold adaptation during deep sleep
 *
 * WHAT: Prevent threshold changes during consolidation
 * WHY:  Preserve learning window for memory consolidation
 * HOW:  Set adaptation rate to near-zero during deep NREM
 *
 * BIOLOGICAL: Deep NREM is consolidation window, not learning window
 *
 * @param bridge Sleep-metaplasticity bridge
 * @return 0 on success, -1 on error
 */
int metaplasticity_sleep_freeze_adaptation(metaplasticity_sleep_bridge_t* bridge);

/**
 * @brief Get sleep-modulated adaptation rate
 *
 * WHAT: Calculate effective threshold adaptation rate for sleep state
 * WHY:  Sleep state controls how fast thresholds track activity
 * HOW:  effective_rate = base_rate × sleep_factor
 *
 * @param bridge Sleep-metaplasticity bridge
 * @param base_rate Original adaptation rate
 * @return Effective adaptation rate
 */
float metaplasticity_sleep_get_adaptation_rate(
    const metaplasticity_sleep_bridge_t* bridge,
    float base_rate
);

/**
 * @brief Get current threshold reset factor
 *
 * WHAT: Retrieve current reset strength for sleep state
 * WHY:  Used to apply graduated reset during sleep stages
 * HOW:  Map sleep state to reset factor
 *
 * @param bridge Sleep-metaplasticity bridge
 * @return Reset factor [0-1]
 */
float metaplasticity_sleep_get_reset_factor(const metaplasticity_sleep_bridge_t* bridge);

/* ============================================================================
 * Metaplasticity → Sleep API
 * ============================================================================ */

/**
 * @brief Monitor threshold drift from baseline
 *
 * WHAT: Track how far thresholds have drifted from baseline
 * WHY:  Excessive drift signals need for homeostatic reset (sleep)
 * HOW:  Compute mean |θ_m - θ_baseline| across population
 *
 * BIOLOGICAL: Accumulated synaptic strength is sleep pressure signal
 *
 * @param bridge Sleep-metaplasticity bridge
 * @param drift Output: current drift magnitude
 * @return 0 on success, -1 on error
 */
int metaplasticity_sleep_monitor_threshold_drift(
    metaplasticity_sleep_bridge_t* bridge,
    float* drift
);

/**
 * @brief Compute sleep pressure contribution from threshold drift
 *
 * WHAT: Calculate how much threshold drift contributes to sleep need
 * WHY:  Threshold drift is homeostatic signal for sleep
 * HOW:  Pressure ∝ (drift / tolerance)
 *
 * BIOLOGICAL: Synaptic Homeostasis Hypothesis - sleep need tracks potentiation
 *
 * @param bridge Sleep-metaplasticity bridge
 * @param sleep_pressure Output: pressure contribution [0-1]
 * @return 0 on success, -1 on error
 */
int metaplasticity_sleep_compute_sleep_pressure(
    metaplasticity_sleep_bridge_t* bridge,
    float* sleep_pressure
);

/**
 * @brief Check if threshold reset is complete
 *
 * WHAT: Determine if thresholds have returned to baseline range
 * WHY:  Sleep quality metric - successful homeostasis restoration
 * HOW:  Check if drift < tolerance
 *
 * @param bridge Sleep-metaplasticity bridge
 * @return true if reset complete
 */
bool metaplasticity_sleep_is_reset_complete(const metaplasticity_sleep_bridge_t* bridge);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update sleep-metaplasticity bridge (both directions)
 *
 * WHAT: Process all sleep-metaplasticity interactions
 * WHY:  Synchronize sleep and threshold dynamics
 * HOW:  Apply reset, freeze adaptation, monitor drift
 *
 * @param bridge Sleep-metaplasticity bridge
 * @return 0 on success, -1 on error
 */
int metaplasticity_sleep_bridge_update(metaplasticity_sleep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current sleep effects on metaplasticity
 *
 * @param bridge Sleep-metaplasticity bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int metaplasticity_sleep_get_effects(
    const metaplasticity_sleep_bridge_t* bridge,
    metaplasticity_sleep_effects_t* effects
);

/**
 * @brief Get current threshold drift statistics
 *
 * @param bridge Sleep-metaplasticity bridge
 * @param mean_drift Output: average drift across population
 * @param max_drift Output: maximum drift observed
 * @return 0 on success, -1 on error
 */
int metaplasticity_sleep_get_drift_stats(
    const metaplasticity_sleep_bridge_t* bridge,
    float* mean_drift,
    float* max_drift
);

/**
 * @brief Check if adaptation is frozen
 *
 * WHAT: Determine if threshold adaptation is paused
 * WHY:  Know if new learning can affect thresholds
 * HOW:  Check if in deep NREM with frozen adaptation
 *
 * @param bridge Sleep-metaplasticity bridge
 * @return true if frozen
 */
bool metaplasticity_sleep_is_adaptation_frozen(const metaplasticity_sleep_bridge_t* bridge);

/**
 * @brief Get total number of sleep resets performed
 *
 * @param bridge Sleep-metaplasticity bridge
 * @return Total reset count
 */
uint64_t metaplasticity_sleep_get_reset_count(const metaplasticity_sleep_bridge_t* bridge);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get reset factor for sleep state
 *
 * WHAT: Map sleep state to threshold reset strength
 * WHY:  Different sleep stages have different reset magnitudes
 * HOW:  Lookup table based on biological evidence
 *
 * @param state Sleep state
 * @return Reset factor [0-1]
 */
float metaplasticity_sleep_state_to_reset_factor(sleep_state_t state);

/**
 * @brief Get adaptation factor for sleep state
 *
 * WHAT: Map sleep state to adaptation rate modulation
 * WHY:  Different sleep stages permit different adaptation speeds
 * HOW:  Lookup table based on consolidation requirements
 *
 * @param state Sleep state
 * @return Adaptation factor [0-1]
 */
float metaplasticity_sleep_state_to_adapt_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METAPLASTICITY_SLEEP_BRIDGE_H */
