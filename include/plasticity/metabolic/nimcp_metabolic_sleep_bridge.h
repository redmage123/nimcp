/**
 * @file nimcp_metabolic_sleep_bridge.h
 * @brief Sleep-Metabolic Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between sleep/wake system and metabolic plasticity
 * WHY:  Sleep states fundamentally alter ATP recovery and energy restoration
 * HOW:  Sleep state modulates recovery rate, deep NREM accelerates ATP restoration
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → METABOLIC PATHWAYS:
 * ---------------------------
 * 1. ATP Recovery During Sleep:
 *    - AWAKE: Standard mitochondrial recovery (2.0 ATP/sec)
 *    - DROWSY: Slightly enhanced recovery (1.3x base)
 *    - LIGHT_NREM: Enhanced recovery (1.8x base)
 *    - DEEP_NREM: Maximum recovery (2.5x base) - synaptic downscaling frees energy
 *    - REM: Standard recovery (1.0x base) - high metabolic activity during replay
 *    - Reference: Vyazovskiy et al. (2008) "Molecular and electrophysiological sleep homeostasis"
 *
 * 2. Synaptic Downscaling in Sleep:
 *    - Deep NREM: Synaptic weights downscaled by 10-20%
 *    - Reduces baseline energy consumption
 *    - Frees ATP for recovery and restoration
 *    - Prepares system for next wake period
 *    - Reference: Tononi & Cirelli (2014) "Sleep function and synaptic homeostasis"
 *
 * 3. Glycogen Restoration:
 *    - NREM sleep: Astrocytes replenish glycogen stores
 *    - Glycogen → lactate → neuronal ATP
 *    - Critical for sustained waking activity
 *    - Reference: Magistretti & Allaman (2015) "Neuron-glia metabolic coupling"
 *
 * 4. Sleep Deprivation Effects:
 *    - Prolonged waking → ATP depletion
 *    - Impaired LTP with sleep deprivation
 *    - Recovery sleep restores plasticity capacity
 *    - Reference: Campbell et al. (2002) "Sleep deprivation impairs long-term potentiation"
 *
 * METABOLIC → SLEEP PATHWAYS:
 * ---------------------------
 * 1. ATP Depletion Drives Sleep Pressure:
 *    - Low ATP → increased adenosine
 *    - Adenosine promotes sleep onset
 *    - Metabolic debt accumulates during wake
 *
 * 2. Energy Depletion Signals:
 *    - Critical ATP levels trigger sleep need
 *    - Feedback to sleep system for state transitions
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                SLEEP-METABOLIC INTEGRATION BRIDGE                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      Recovery    Effect                                     ║
 * ║   ──────────────────────────────────────────────────────────────────      ║
 * ║   AWAKE            1.0x         Standard mitochondrial ATP production     ║
 * ║   DROWSY           1.3x         Transition to restoration mode            ║
 * ║   LIGHT_NREM       1.8x         Enhanced recovery, selective replay       ║
 * ║   DEEP_NREM        2.5x         Maximum restoration, downscaling          ║
 * ║   REM              1.0x         Active replay, standard recovery          ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_METABOLIC_SLEEP_BRIDGE_H
#define NIMCP_METABOLIC_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/metabolic/nimcp_metabolic_plasticity.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Metabolic Modulation
 * ============================================================================ */

/* ATP recovery rate modulation by sleep state */
#define METABOLIC_SLEEP_RECOVERY_AWAKE         1.0f   /**< Standard recovery */
#define METABOLIC_SLEEP_RECOVERY_DROWSY        1.3f   /**< Slightly enhanced */
#define METABOLIC_SLEEP_RECOVERY_LIGHT_NREM    1.8f   /**< Enhanced recovery */
#define METABOLIC_SLEEP_RECOVERY_DEEP_NREM     2.5f   /**< Maximum restoration */
#define METABOLIC_SLEEP_RECOVERY_REM           1.0f   /**< Standard (active replay) */

/* Energy cost modulation by sleep state */
#define METABOLIC_SLEEP_COST_AWAKE             1.0f   /**< Standard costs */
#define METABOLIC_SLEEP_COST_DROWSY            0.8f   /**< Reduced activity */
#define METABOLIC_SLEEP_COST_LIGHT_NREM        0.5f   /**< Low activity */
#define METABOLIC_SLEEP_COST_DEEP_NREM         0.3f   /**< Minimal activity */
#define METABOLIC_SLEEP_COST_REM               1.2f   /**< Increased (replay) */

/* Sleep pressure thresholds based on ATP */
#define METABOLIC_SLEEP_PRESSURE_ATP_LOW       40.0f  /**< ATP < 40% → high sleep pressure */
#define METABOLIC_SLEEP_PRESSURE_ATP_CRITICAL  20.0f  /**< ATP < 20% → critical sleep need */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-metabolic bridge configuration
 */
typedef struct {
    bool enable_recovery_modulation;  /**< Enable recovery rate changes */
    bool enable_cost_modulation;      /**< Enable energy cost changes */
    bool enable_sleep_pressure_feedback; /**< Enable ATP → sleep pressure feedback */
    float modulation_strength;        /**< Overall modulation strength (0-1) */
} metabolic_sleep_config_t;

/**
 * @brief Computed sleep effects on metabolic plasticity
 */
typedef struct {
    float recovery_rate_factor;   /**< Multiply recovery rate by this */
    float energy_cost_factor;     /**< Multiply energy costs by this */
    sleep_state_t current_state;  /**< Current sleep state */
    float sleep_pressure;         /**< Current sleep pressure */
    bool deep_restoration_active; /**< True during deep NREM */
    float effective_recovery_rate; /**< Actual recovery rate (ATP/sec) */
} metabolic_sleep_effects_t;

/**
 * @brief Sleep-metabolic integration bridge
 */
typedef struct metabolic_sleep_bridge_struct* metabolic_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default sleep-metabolic bridge configuration
 *
 * WHAT: Provide sensible defaults based on sleep restoration research
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int metabolic_sleep_default_config(metabolic_sleep_config_t* config);

/**
 * @brief Create sleep-metabolic bridge
 *
 * WHAT: Initialize integration between sleep and metabolic systems
 * WHY:  Enable realistic sleep-based energy restoration
 * HOW:  Allocate structure, link systems, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param sleep_system Sleep/wake system
 * @param metabolic Metabolic plasticity system
 * @return New bridge or NULL on failure
 */
metabolic_sleep_bridge_t metabolic_sleep_bridge_create(
    const metabolic_sleep_config_t* config,
    sleep_system_t sleep_system,
    metabolic_plasticity_t* metabolic
);

/**
 * @brief Destroy sleep-metabolic bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister callbacks, free structure
 *
 * @param bridge Bridge to destroy
 */
void metabolic_sleep_bridge_destroy(metabolic_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update metabolic effects from sleep system state
 *
 * WHAT: Compute how current sleep state affects metabolic parameters
 * WHY:  Apply sleep modulation to energy dynamics
 * HOW:  Query sleep state, update recovery rate and costs
 *
 * @param bridge Sleep-metabolic bridge
 * @return 0 on success
 */
int metabolic_sleep_update(metabolic_sleep_bridge_t bridge);

/**
 * @brief Get modulated metabolic parameters for current sleep state
 *
 * WHAT: Query current sleep effects on metabolism
 * WHY:  Monitor sleep-metabolic coupling
 * HOW:  Copy current effects to output struct
 *
 * @param bridge Sleep-metabolic bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int metabolic_sleep_get_effects(const metabolic_sleep_bridge_t bridge,
                                 metabolic_sleep_effects_t* effects);

/**
 * @brief Get sleep-modulated ATP recovery rate
 *
 * WHAT: Calculate effective recovery rate for current sleep state
 * WHY:  Apply sleep modulation to ATP restoration
 * HOW:  Multiply base rate by sleep state factor
 *
 * @param bridge Sleep-metabolic bridge
 * @param base_rate Base recovery rate (ATP/sec)
 * @return Effective recovery rate
 */
float metabolic_sleep_get_recovery_rate(const metabolic_sleep_bridge_t bridge,
                                         float base_rate);

/**
 * @brief Get sleep-modulated energy cost factor
 *
 * WHAT: Calculate cost scaling for current sleep state
 * WHY:  Reduced activity during sleep lowers energy costs
 * HOW:  Return sleep state cost multiplier
 *
 * @param bridge Sleep-metabolic bridge
 * @return Cost scaling factor [0.3-1.2]
 */
float metabolic_sleep_get_cost_factor(const metabolic_sleep_bridge_t bridge);

/**
 * @brief Check if deep restoration is active
 *
 * WHAT: Determine if in deep NREM restoration mode
 * WHY:  Deep NREM has special metabolic properties
 * HOW:  Check if current state is DEEP_NREM
 *
 * @param bridge Sleep-metabolic bridge
 * @return true if in deep restoration
 */
bool metabolic_sleep_is_deep_restoration(const metabolic_sleep_bridge_t bridge);

/* ============================================================================
 * Feedback API (Metabolic → Sleep)
 * ============================================================================ */

/**
 * @brief Get sleep pressure from ATP depletion
 *
 * WHAT: Calculate sleep pressure based on ATP level
 * WHY:  ATP depletion drives sleep need (adenosine accumulation)
 * HOW:  Map ATP level to sleep pressure [0-1]
 *
 * @param bridge Sleep-metabolic bridge
 * @return Sleep pressure [0-1]
 */
float metabolic_sleep_get_sleep_pressure(const metabolic_sleep_bridge_t bridge);

/**
 * @brief Check if critical sleep need due to ATP depletion
 *
 * WHAT: Determine if ATP is critically low
 * WHY:  Critical depletion should trigger forced sleep
 * HOW:  Check if ATP < critical threshold
 *
 * @param bridge Sleep-metabolic bridge
 * @return true if critical sleep need
 */
bool metabolic_sleep_is_critical_need(const metabolic_sleep_bridge_t bridge);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get recovery rate factor for sleep state
 *
 * WHAT: Map sleep state to recovery rate multiplier
 * WHY:  Centralized state → factor mapping
 * HOW:  Return constant for each state
 *
 * @param state Sleep state
 * @return Recovery rate factor
 */
float metabolic_sleep_get_recovery_factor(sleep_state_t state);

/**
 * @brief Get energy cost factor for sleep state
 *
 * WHAT: Map sleep state to cost multiplier
 * WHY:  Different states have different activity levels
 * HOW:  Return constant for each state
 *
 * @param state Sleep state
 * @return Energy cost factor
 */
float metabolic_sleep_get_energy_cost_factor(sleep_state_t state);

/**
 * @brief Calculate sleep pressure from ATP level
 *
 * WHAT: Convert ATP percentage to sleep pressure
 * WHY:  Standardized ATP → pressure mapping
 * HOW:  Inverse relationship (low ATP = high pressure)
 *
 * @param atp_level Current ATP level [0-100]
 * @return Sleep pressure [0-1]
 */
float metabolic_sleep_atp_to_pressure(float atp_level);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METABOLIC_SLEEP_BRIDGE_H */
