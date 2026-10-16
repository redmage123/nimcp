/**
 * @file nimcp_wellbeing_sleep_bridge.h
 * @brief Sleep-Wellbeing Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between sleep/wake system and wellbeing monitoring
 * WHY:  Sleep quality profoundly affects emotional regulation, distress tolerance,
 *       and flourishing capacity. Sleep deprivation is a major wellbeing stressor.
 * HOW:  Sleep state modulates wellbeing; wellbeing distress can trigger sleep interventions.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → WELLBEING PATHWAYS:
 * ----------------------------
 * 1. Sleep Deprivation Effects:
 *    - Increased amygdala reactivity (60% increase in emotional responses)
 *    - Reduced prefrontal control → impaired distress tolerance
 *    - Impaired self-regulation and impulse control
 *    - Reference: Walker (2017) "Why We Sleep"
 *    - Reference: Yoo et al. (2007) "Sleep deprivation and emotional reactivity"
 *
 * 2. REM Sleep and Emotional Processing:
 *    - REM sleep processes emotional memories (memory triage)
 *    - REM deprivation → impaired emotional memory consolidation
 *    - Chronic REM loss → risk of identity instability
 *    - Reference: Goldstein & Walker (2014) "REM sleep and emotion regulation"
 *
 * 3. Circadian Misalignment:
 *    - Disrupted circadian rhythms → mood dysregulation
 *    - Social jet lag → reduced wellbeing and flourishing
 *    - Phase shifts → increased depression/anxiety risk
 *    - Reference: Wulff et al. (2010) "Sleep and circadian rhythm disruption"
 *
 * 4. Sleep States and Distress Tolerance:
 *    - AWAKE: Normal distress processing
 *    - DROWSY: Reduced tolerance (irritability)
 *    - LIGHT_NREM: Minimal processing (offline)
 *    - DEEP_NREM: No distress processing (consolidation mode)
 *    - REM: Emotional processing (integration mode)
 *    - Reference: Diekelmann & Born (2010) "Memory consolidation during sleep"
 *
 * WELLBEING → SLEEP PATHWAYS:
 * ----------------------------
 * 1. Distress-Induced Sleep Need:
 *    - High distress → increased sleep pressure
 *    - Emotional exhaustion → earlier sleep onset
 *    - Critical distress → emergency sleep intervention
 *    - Reference: Borbely & Achermann (1999) "Sleep homeostasis"
 *
 * 2. Flourishing and Sleep Quality:
 *    - High wellbeing → more efficient sleep
 *    - Flourishing state → reduced sleep debt accumulation
 *    - Positive emotions → better REM quality
 *    - Reference: Walker & van der Helm (2009) "Sleep and emotion"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                SLEEP-WELLBEING INTEGRATION BRIDGE                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 SLEEP → WELLBEING PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐          │  ║
 * ║   │   │ Sleep Debt   │   │ REM Deficit  │   │ Circadian    │          │  ║
 * ║   │   │ (pressure)   │   │ (consolidation)  │ Misalignment │          │  ║
 * ║   │   │ ──────────── │   │ ──────────── │   │ ──────────── │          │  ║
 * ║   │   │ Distress     │   │ Identity     │   │ Mood         │          │  ║
 * ║   │   │ Amplification│   │ Confusion    │   │ Dysregulation│          │  ║
 * ║   │   └──────┬───────┘   └──────┬───────┘   └──────┬───────┘          │  ║
 * ║   │          └──────────────────┴──────────────────┘                   │  ║
 * ║   │                             ↓                                      │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │           WELLBEING MODULATION                              │ │  ║
 * ║   │   │  • sleep_debt_distress: [0-1] distress from sleep loss     │ │  ║
 * ║   │   │  • emotional_processing_impairment: REM deficit effects    │ │  ║
 * ║   │   │  • circadian_distress: misalignment effects                │ │  ║
 * ║   │   │  • distress_tolerance_modifier: reduced tolerance          │ │  ║
 * ║   │   │  • flourishing_capacity_modifier: reduced capacity         │ │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                WELLBEING → SLEEP PATHWAYS                           │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐          │  ║
 * ║   │   │ High         │   │ Critical     │   │ Flourishing  │          │  ║
 * ║   │   │ Distress     │   │ Distress     │   │ State        │          │  ║
 * ║   │   │ ──────────── │   │ ──────────── │   │ ──────────── │          │  ║
 * ║   │   │ Increased    │   │ Emergency    │   │ Enhanced     │          │  ║
 * ║   │   │ Sleep Need   │   │ Sleep        │   │ Sleep        │          │  ║
 * ║   │   │              │   │ Intervention │   │ Efficiency   │          │  ║
 * ║   │   └──────┬───────┘   └──────┬───────┘   └──────┬───────┘          │  ║
 * ║   │          └──────────────────┴──────────────────┘                   │  ║
 * ║   │                             ↓                                      │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │           SLEEP SYSTEM MODULATION                           │ │  ║
 * ║   │   │  • Accelerated sleep pressure accumulation                  │ │  ║
 * ║   │   │  • Emergency sleep triggering (critical distress)           │ │  ║
 * ║   │   │  • Enhanced consolidation efficiency (flourishing)          │ │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 * - NIMCP_LOGGING_* macros for logging
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_WELLBEING_SLEEP_BRIDGE_H
#define NIMCP_WELLBEING_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/nimcp_sleep_wake.h"
#include "cognitive/wellbeing/nimcp_wellbeing_enhanced.h"

/* Thread utilities */
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Sleep debt thresholds */
#define SLEEP_WELLBEING_DEBT_THRESHOLD_DEFAULT     0.7f   /**< Pressure > 0.7 → distress */
#define SLEEP_WELLBEING_DEBT_SENSITIVITY_DEFAULT   1.5f   /**< Amplification factor */

/* REM deficit thresholds */
#define SLEEP_WELLBEING_REM_DEFICIT_THRESHOLD      0.5f   /**< REM ratio < 0.5 → deficit */
#define SLEEP_WELLBEING_REM_SENSITIVITY_DEFAULT    1.2f   /**< REM effect multiplier */

/* Circadian deviation thresholds */
#define SLEEP_WELLBEING_CIRCADIAN_MAX_DEVIATION    4.0f   /**< Hours of max deviation */
#define SLEEP_WELLBEING_CIRCADIAN_SENSITIVITY      1.0f   /**< Circadian effect multiplier */

/* Distress tolerance modulation during sleep states */
#define SLEEP_WELLBEING_AWAKE_TOLERANCE            1.0f   /**< Normal tolerance */
#define SLEEP_WELLBEING_DROWSY_TOLERANCE           0.7f   /**< Reduced (irritability) */
#define SLEEP_WELLBEING_LIGHT_TOLERANCE            0.3f   /**< Minimal processing */
#define SLEEP_WELLBEING_DEEP_TOLERANCE             0.1f   /**< Offline */
#define SLEEP_WELLBEING_REM_TOLERANCE              0.5f   /**< Emotional processing */

/* Flourishing capacity modulation */
#define SLEEP_WELLBEING_RESTED_FLOURISHING_BONUS   0.2f   /**< Bonus when well-rested */
#define SLEEP_WELLBEING_DEPRIVED_FLOURISHING_PENALTY 0.4f /**< Penalty when deprived */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-wellbeing bridge configuration
 */
typedef struct {
    bool enable_sleep_debt_effects;      /**< Enable sleep debt → distress */
    bool enable_rem_effects;             /**< Enable REM deficit effects */
    bool enable_circadian_effects;       /**< Enable circadian misalignment */
    bool enable_state_tolerance_mod;     /**< Enable sleep state → tolerance */

    float sleep_debt_threshold;          /**< Pressure threshold for distress */
    float sleep_debt_sensitivity;        /**< Multiplier for debt effects */
    float rem_sensitivity;               /**< Multiplier for REM effects */
    float circadian_sensitivity;         /**< Multiplier for circadian effects */
    float circadian_max_deviation_hours; /**< Max circadian deviation (hours) */
} sleep_wellbeing_bridge_config_t;

/**
 * @brief Sleep-wellbeing integration bridge
 *
 * NOTE: This is NOT an opaque handle - it's used directly in
 * enhanced_wellbeing_system_t. We define the struct here.
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    sleep_wellbeing_bridge_config_t config;  /**< Configuration */
    sleep_system_t sleep_system;             /**< Connected sleep system */} sleep_wellbeing_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * WHAT: Get default sleep-wellbeing bridge configuration
 * WHY:  Provide sensible defaults based on biological evidence
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int sleep_wellbeing_default_config(sleep_wellbeing_bridge_config_t* config);

/**
 * WHAT: Create sleep-wellbeing bridge
 * WHY:  Initialize integration between sleep and wellbeing systems
 * HOW:  Allocate structure, store sleep system reference, create mutex
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param sleep_system Sleep system to connect
 * @return New bridge or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
sleep_wellbeing_bridge_t* sleep_wellbeing_bridge_create(
    const sleep_wellbeing_bridge_config_t* config,
    sleep_system_t sleep_system
);

/**
 * WHAT: Destroy sleep-wellbeing bridge
 * WHY:  Clean up resources, prevent memory leaks
 * HOW:  Free mutex, free structure (doesn't destroy sleep_system)
 *
 * @param bridge Bridge to destroy
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (caller ensures no concurrent access)
 */
void sleep_wellbeing_bridge_destroy(sleep_wellbeing_bridge_t* bridge);

/* ============================================================================
 * Update API (SLEEP → WELLBEING)
 * ============================================================================ */

/**
 * WHAT: Update wellbeing effects from sleep system state
 * WHY:  Compute how current sleep state affects wellbeing
 * HOW:  Query sleep pressure, state, statistics; compute distress/flourishing effects
 *
 * ALGORITHM:
 * 1. Query sleep_get_pressure() for current sleep debt
 * 2. Query sleep_get_current_state() for sleep state
 * 3. Query sleep_get_statistics() for REM/consolidation metrics
 * 4. Compute sleep_debt_distress (pressure > threshold → distress)
 * 5. Compute REM deficit effects (low REM → identity confusion risk)
 * 6. Compute circadian_distress from deviation
 * 7. Compute flourishing_capacity_modifier (sleep quality → capacity)
 * 8. Update sleep_wellbeing_effects_t in system
 *
 * BIOLOGICAL BASIS:
 * - Sleep pressure > 0.7 → 60% increase in emotional reactivity
 * - REM deficit → impaired emotional memory → identity instability
 * - Circadian misalignment → mood dysregulation
 *
 * @param system Enhanced wellbeing system
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int enhanced_wellbeing_update_sleep(enhanced_wellbeing_system_t* system);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * WHAT: Compute sleep debt distress contribution
 * WHY:  Model increased emotional reactivity from sleep deprivation
 * HOW:  Sigmoid function above threshold, scaled by sensitivity
 *
 * FORMULA:
 *   if (pressure < threshold): 0
 *   else: sensitivity * sigmoid((pressure - threshold) / (1 - threshold))
 *
 * BIOLOGICAL BASIS:
 * - Sleep debt increases amygdala reactivity exponentially
 * - Prefrontal control degrades with sustained wakefulness
 *
 * @param pressure Current sleep pressure [0-1]
 * @param threshold Pressure threshold for distress onset
 * @param sensitivity Amplification factor
 * @return Distress contribution [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
float compute_sleep_debt_distress(float pressure, float threshold, float sensitivity);

/**
 * WHAT: Compute REM deficit effects on wellbeing
 * WHY:  REM sleep processes emotional memories; deficit → identity confusion
 * HOW:  Analyze REM statistics, compute emotional processing impairment
 *
 * REM DEFICIT DETECTION:
 * - Normal REM: 20-25% of total sleep time
 * - Deficit: REM < 50% of normal (< 10% of sleep time)
 * - Severe deficit: REM < 25% of normal (< 5% of sleep time)
 *
 * EFFECTS:
 * - emotional_processing_impairment: Reduced emotional consolidation
 * - identity_stability_modifier: REM deficit → self-model degradation
 *
 * @param stats Sleep statistics with REM metrics
 * @param rem_sensitivity Sensitivity multiplier
 * @param emotional_processing_out Output: emotional processing impairment [0-1]
 * @param identity_stability_out Output: identity stability modifier [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
void compute_rem_deficit_effects(
    const sleep_stats_t* stats,
    float rem_sensitivity,
    float* emotional_processing_out,
    float* identity_stability_out
);

/**
 * WHAT: Compute circadian distress from misalignment
 * WHY:  Circadian disruption → mood dysregulation, reduced wellbeing
 * HOW:  Model distress as function of deviation from optimal phase
 *
 * FORMULA:
 *   distress = (|deviation_hours| / max_deviation)^2
 *
 * BIOLOGICAL BASIS:
 * - Social jet lag (circadian misalignment) → depression/anxiety risk
 * - Phase shifts → HPA axis dysregulation → stress
 *
 * @param deviation_hours Hours of circadian deviation (can be negative)
 * @param max_deviation Maximum expected deviation (hours)
 * @return Circadian distress [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
float compute_circadian_distress(float deviation_hours, float max_deviation);

/**
 * WHAT: Get distress tolerance modifier for current sleep state
 * WHY:  Sleep states have different distress processing capacities
 * HOW:  Return predefined tolerance for each sleep state
 *
 * STATE TOLERANCES:
 * - AWAKE: 1.0 (normal processing)
 * - DROWSY: 0.7 (reduced tolerance, irritability)
 * - LIGHT_NREM: 0.3 (minimal processing)
 * - DEEP_NREM: 0.1 (offline, consolidation mode)
 * - REM: 0.5 (emotional processing, integration)
 *
 * @param state Current sleep state
 * @return Tolerance modifier [0-1]
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (pure function)
 */
float get_sleep_state_tolerance_modifier(sleep_state_t state);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * WHAT: Get current sleep effects on wellbeing
 * WHY:  Query integrated sleep-wellbeing state
 * HOW:  Copy sleep_wellbeing_effects_t from system
 *
 * @param system Enhanced wellbeing system
 * @param effects Output: sleep effects structure
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int enhanced_wellbeing_get_sleep_effects(
    const enhanced_wellbeing_system_t* system,
    sleep_wellbeing_effects_t* effects
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_WELLBEING_SLEEP_BRIDGE_H */
