/**
 * @file nimcp_astrocyte_sleep_bridge.h
 * @brief Sleep-Astrocyte Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between sleep/wake system and astrocyte plasticity
 * WHY:  Sleep states profoundly affect astrocyte function and gliotransmitter release
 * HOW:  Sleep state modulates D-serine release, glutamate uptake, ATP signaling
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → ASTROCYTE PATHWAYS:
 * ----------------------------
 * 1. NREM Sleep Enhancement of Gliotransmission:
 *    - AWAKE: Noradrenaline suppresses astrocyte calcium signaling
 *    - NREM: Reduced noradrenaline → increased astrocyte calcium
 *    - NREM: Peak D-serine release for memory consolidation
 *    - Deep NREM: Enhanced gliotransmitter tone supports synaptic homeostasis
 *    Reference: Poskanzer & Yuste (2016) "Astrocytes regulate cortical state"
 *               Ding et al. (2016) "Changes in astrocyte-neuron ratio with sleep"
 *
 * 2. Noradrenaline Gating of Astrocyte Activity:
 *    - Awake: High noradrenaline → α1-adrenergic receptor activation
 *    - α1-AR activation suppresses astrocyte calcium oscillations
 *    - NREM: Low noradrenaline → disinhibition of astrocyte calcium
 *    - Enables sleep-specific gliotransmitter release patterns
 *    Reference: Paukert et al. (2014) "Norepinephrine controls astroglial responsiveness"
 *
 * 3. Glymphatic Clearance:
 *    - NREM sleep: Astrocyte volume decreases ~60%
 *    - Increased interstitial space (ISF volume ↑ 60%)
 *    - Enhanced clearance of metabolites (β-amyloid, lactate)
 *    - Improved glutamate uptake efficiency during sleep
 *    Reference: Xie et al. (2013) "Sleep drives metabolite clearance"
 *               Iliff et al. (2012) "Glymphatic pathway"
 *
 * 4. Sleep-Dependent D-Serine Release:
 *    - NREM: 40% increase in D-serine availability
 *    - Supports NMDA-dependent memory consolidation
 *    - Enables synaptic replay with full LTP capacity
 *    - REM: Reduced D-serine (supports depotentiation)
 *    Reference: Henneberger et al. (2010) "D-serine and LTP"
 *
 * 5. ATP/Adenosine Sleep Homeostasis:
 *    - Prolonged wake: ATP accumulation in astrocytes
 *    - ATP → adenosine conversion increases with wake duration
 *    - Adenosine A1R activation promotes sleep pressure
 *    - NREM sleep: Adenosine clearance, ATP restoration
 *    Reference: Halassa et al. (2009) "Astrocytic modulation of sleep"
 *               Porkka-Heiskanen & Kalinchuk (2011) "Adenosine homeostasis"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-ASTROCYTE INTEGRATION BRIDGE                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE   D-Serine   Glu_Uptake   ATP      Effect                  ║
 * ║   ──────────────────────────────────────────────────────────────────────  ║
 * ║   AWAKE         0.8        0.9          0.2      Norad suppression        ║
 * ║   DROWSY        0.9        0.92         0.3      Transitioning            ║
 * ║   LIGHT_NREM    1.1        0.95         0.1      Enhanced consolidation   ║
 * ║   DEEP_NREM     1.2        0.98         0.05     Peak glymphatic          ║
 * ║   REM           0.7        0.88         0.15     Reduced D-serine         ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_ASTROCYTE_SLEEP_BRIDGE_H
#define NIMCP_ASTROCYTE_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/astrocyte/nimcp_astrocyte_plasticity.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Astrocyte Modulation
 * ============================================================================ */

/* D-serine modulation by sleep state */
#define ASTROCYTE_SLEEP_D_SERINE_AWAKE       0.8f   /**< Baseline awake */
#define ASTROCYTE_SLEEP_D_SERINE_DROWSY      0.9f   /**< Transition up */
#define ASTROCYTE_SLEEP_D_SERINE_LIGHT_NREM  1.1f   /**< Enhanced */
#define ASTROCYTE_SLEEP_D_SERINE_DEEP_NREM   1.2f   /**< Peak release */
#define ASTROCYTE_SLEEP_D_SERINE_REM         0.7f   /**< Reduced */

/* Glutamate uptake modulation by sleep state */
#define ASTROCYTE_SLEEP_GLU_UPTAKE_AWAKE       0.90f  /**< Normal */
#define ASTROCYTE_SLEEP_GLU_UPTAKE_DROWSY      0.92f  /**< Slightly better */
#define ASTROCYTE_SLEEP_GLU_UPTAKE_LIGHT_NREM  0.95f  /**< Enhanced */
#define ASTROCYTE_SLEEP_GLU_UPTAKE_DEEP_NREM   0.98f  /**< Peak glymphatic */
#define ASTROCYTE_SLEEP_GLU_UPTAKE_REM         0.88f  /**< Reduced */

/* ATP/Adenosine modulation by sleep state */
#define ASTROCYTE_SLEEP_ATP_AWAKE        0.2f   /**< Baseline */
#define ASTROCYTE_SLEEP_ATP_DROWSY       0.3f   /**< Accumulating */
#define ASTROCYTE_SLEEP_ATP_LIGHT_NREM   0.1f   /**< Clearing */
#define ASTROCYTE_SLEEP_ATP_DEEP_NREM    0.05f  /**< Minimal */
#define ASTROCYTE_SLEEP_ATP_REM          0.15f  /**< Moderate */

/* Calcium wave modulation */
#define ASTROCYTE_SLEEP_CA_WAVE_AWAKE      0.3f   /**< Suppressed by norad */
#define ASTROCYTE_SLEEP_CA_WAVE_NREM       1.0f   /**< Full activity */
#define ASTROCYTE_SLEEP_CA_WAVE_REM        0.5f   /**< Moderate */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-astrocyte bridge configuration
 */
typedef struct {
    bool enable_d_serine_modulation;    /**< Modulate D-serine by sleep state */
    bool enable_uptake_modulation;      /**< Modulate glutamate uptake */
    bool enable_atp_modulation;         /**< Modulate ATP/adenosine */
    bool enable_calcium_modulation;     /**< Modulate calcium waves */
    float modulation_strength;          /**< Overall modulation strength (0-1) */
} astrocyte_sleep_config_t;

/**
 * @brief Computed sleep effects on astrocyte function
 */
typedef struct {
    float d_serine_factor;              /**< D-serine level multiplier */
    float glutamate_uptake_factor;      /**< Uptake rate multiplier */
    float atp_release_factor;           /**< ATP release multiplier */
    float calcium_wave_factor;          /**< Calcium activity multiplier */
    sleep_state_t current_state;        /**< Current sleep state */
    float sleep_pressure;               /**< Current sleep pressure */
    bool glymphatic_active;             /**< Glymphatic clearance active */
} astrocyte_sleep_effects_t;

/**
 * @brief Sleep-astrocyte integration bridge
 */
typedef struct astrocyte_sleep_bridge_struct* astrocyte_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default sleep-astrocyte bridge configuration
 *
 * WHAT: Provide sensible defaults based on sleep neuroscience
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int astrocyte_sleep_default_config(astrocyte_sleep_config_t* config);

/**
 * @brief Create sleep-astrocyte bridge
 *
 * WHAT: Initialize integration between sleep and astrocyte systems
 * WHY:  Enable sleep-dependent astrocyte modulation
 * HOW:  Allocate structure, link systems, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param sleep_system Sleep/wake system
 * @param astrocyte_system Astrocyte plasticity system
 * @return New bridge or NULL on failure
 */
astrocyte_sleep_bridge_t astrocyte_sleep_bridge_create(
    const astrocyte_sleep_config_t* config,
    sleep_system_t sleep_system,
    astrocyte_plasticity_t astrocyte_system
);

/**
 * @brief Destroy sleep-astrocyte bridge
 *
 * WHAT: Clean up resources and unregister callbacks
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void astrocyte_sleep_bridge_destroy(astrocyte_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update astrocyte effects from sleep system state
 *
 * WHAT: Compute how current sleep state affects astrocytes
 * WHY:  Astrocyte function varies dramatically across sleep states
 * HOW:  Query sleep state, map to astrocyte modulation factors
 *
 * @param bridge Sleep-astrocyte bridge
 * @return 0 on success
 */
int astrocyte_sleep_update(astrocyte_sleep_bridge_t bridge);

/**
 * @brief Apply sleep modulation to all astrocytes
 *
 * WHAT: Update all astrocyte states based on sleep effects
 * WHY:  Realize sleep-dependent changes in gliotransmission
 * HOW:  Iterate astrocytes, apply modulation factors
 *
 * @param bridge Sleep-astrocyte bridge
 * @return 0 on success
 */
int astrocyte_sleep_apply_modulation(astrocyte_sleep_bridge_t bridge);

/**
 * @brief Get current sleep effects on astrocytes
 *
 * WHAT: Retrieve computed modulation factors
 * WHY:  Query current sleep-astrocyte state
 * HOW:  Copy effects structure
 *
 * @param bridge Sleep-astrocyte bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int astrocyte_sleep_get_effects(const astrocyte_sleep_bridge_t bridge,
                                 astrocyte_sleep_effects_t* effects);

/**
 * @brief Get sleep-modulated D-serine level
 *
 * WHAT: Calculate effective D-serine with sleep modulation
 * WHY:  D-serine peaks during NREM for consolidation
 * HOW:  Multiply base level by sleep factor
 *
 * @param bridge Sleep-astrocyte bridge
 * @param base_d_serine Base D-serine level
 * @return Modulated D-serine level
 */
float astrocyte_sleep_get_d_serine_level(const astrocyte_sleep_bridge_t bridge,
                                          float base_d_serine);

/**
 * @brief Get sleep-modulated glutamate uptake rate
 *
 * WHAT: Calculate effective uptake rate with sleep modulation
 * WHY:  Glymphatic system enhances uptake during NREM
 * HOW:  Multiply base rate by sleep factor
 *
 * @param bridge Sleep-astrocyte bridge
 * @param base_uptake Base uptake rate
 * @return Modulated uptake rate
 */
float astrocyte_sleep_get_glutamate_uptake(const astrocyte_sleep_bridge_t bridge,
                                            float base_uptake);

/**
 * @brief Check if glymphatic clearance is active
 *
 * WHAT: Determine if glymphatic system is clearing metabolites
 * WHY:  Glymphatic active mainly during NREM sleep
 * HOW:  Check sleep state (LIGHT_NREM or DEEP_NREM)
 *
 * @param bridge Sleep-astrocyte bridge
 * @return true if glymphatic is active
 */
bool astrocyte_sleep_is_glymphatic_active(const astrocyte_sleep_bridge_t bridge);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get D-serine modulation factor for sleep state
 *
 * @param state Sleep state
 * @return D-serine multiplier
 */
float astrocyte_sleep_get_d_serine_factor(sleep_state_t state);

/**
 * @brief Get glutamate uptake modulation factor for sleep state
 *
 * @param state Sleep state
 * @return Uptake rate multiplier
 */
float astrocyte_sleep_get_uptake_factor(sleep_state_t state);

/**
 * @brief Get ATP release modulation factor for sleep state
 *
 * @param state Sleep state
 * @return ATP release multiplier
 */
float astrocyte_sleep_get_atp_factor(sleep_state_t state);

/**
 * @brief Get calcium wave modulation factor for sleep state
 *
 * @param state Sleep state
 * @return Calcium activity multiplier
 */
float astrocyte_sleep_get_calcium_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ASTROCYTE_SLEEP_BRIDGE_H */
