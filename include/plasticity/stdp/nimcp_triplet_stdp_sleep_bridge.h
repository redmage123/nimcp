/**
 * @file nimcp_triplet_stdp_sleep_bridge.h
 * @brief Sleep-Triplet STDP Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between sleep/wake system and triplet STDP
 * WHY:  Sleep states modulate triplet spike trace dynamics and frequency-dependent plasticity
 * HOW:  Sleep state affects trace time constants (tau_plus, tau_minus, tau_x, tau_y)
 *       and learning amplitudes (A2_plus, A3_plus, A2_minus, A3_minus)
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP → TRIPLET STDP PATHWAYS:
 * -------------------------------
 * 1. Trace Time Constant Modulation:
 *    - AWAKE: Normal trace dynamics (tau_plus=16.8ms, tau_x=101ms)
 *    - NREM: Accelerated trace decay (favor recent spikes, downscaling)
 *    - REM: Slower decay of slow traces (enhance triplet accumulation)
 *    - Reference: Grosmark et al. (2012) "REM sleep and memory consolidation"
 *
 * 2. Triplet Amplitude Modulation:
 *    - AWAKE: Standard pairwise + triplet terms
 *    - DEEP_NREM: Reduced triplet terms (suppress frequency dependence)
 *    - REM: Enhanced triplet terms (amplify high-frequency effects)
 *    - Models differential consolidation of burst vs. single spikes
 *
 * 3. Frequency Dependence and Sleep:
 *    - Hippocampal replay during NREM: Sharp-wave ripples (150-200 Hz)
 *    - Triplet terms dominate at high frequency → REM enhances triplet LTP
 *    - Synaptic downscaling during NREM: Reduce triplet sensitivity
 *    - Reference: Pfister & Gerstner (2006) "Triplet frequency dependence"
 *
 * 4. Slow Trace Accumulation in Sleep:
 *    - r2_pre and o2_post have long time constants (100-125 ms)
 *    - During REM replay, slow traces accumulate across spike sequences
 *    - Enables integration of spike patterns across time
 *    - Models consolidation of temporal sequences
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              SLEEP-TRIPLET STDP INTEGRATION BRIDGE                         ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE    Fast τ   Slow τ   A2 Factor  A3 Factor  Effect          ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE          1.0      1.0      1.0        1.0        Standard triplet ║
 * ║   DROWSY         1.0      1.0      0.8        0.7        Reduced learning ║
 * ║   LIGHT_NREM     0.8      0.9      0.6        0.5        Faster decay     ║
 * ║   DEEP_NREM      0.7      0.8      0.5        0.3        Downscaling      ║
 * ║   REM            1.2      1.4      1.0        1.5        Triplet boost    ║
 * ║                                                                            ║
 * ║   FAST TRACES (r1, o1): Pairwise STDP component                           ║
 * ║   SLOW TRACES (r2, o2): Triplet/quadruplet component                      ║
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
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TRIPLET_STDP_SLEEP_BRIDGE_H
#define NIMCP_TRIPLET_STDP_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "plasticity/stdp/nimcp_triplet_stdp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants - Sleep State Modulation for Triplet STDP
 * ============================================================================ */

/* Fast trace time constant modulation (tau_plus, tau_minus) */
#define TRIPLET_STDP_SLEEP_TAU_FAST_AWAKE       1.0f   /**< Normal fast traces */
#define TRIPLET_STDP_SLEEP_TAU_FAST_DROWSY      1.0f   /**< Unchanged */
#define TRIPLET_STDP_SLEEP_TAU_FAST_LIGHT_NREM  0.8f   /**< Faster decay */
#define TRIPLET_STDP_SLEEP_TAU_FAST_DEEP_NREM   0.7f   /**< Fastest decay */
#define TRIPLET_STDP_SLEEP_TAU_FAST_REM         1.2f   /**< Slower decay */

/* Slow trace time constant modulation (tau_x, tau_y) */
#define TRIPLET_STDP_SLEEP_TAU_SLOW_AWAKE       1.0f   /**< Normal slow traces */
#define TRIPLET_STDP_SLEEP_TAU_SLOW_DROWSY      1.0f   /**< Unchanged */
#define TRIPLET_STDP_SLEEP_TAU_SLOW_LIGHT_NREM  0.9f   /**< Slightly faster */
#define TRIPLET_STDP_SLEEP_TAU_SLOW_DEEP_NREM   0.8f   /**< Faster decay */
#define TRIPLET_STDP_SLEEP_TAU_SLOW_REM         1.4f   /**< Much slower (accumulation) */

/* Pairwise amplitude modulation (A2_plus, A2_minus) */
#define TRIPLET_STDP_SLEEP_A2_AWAKE             1.0f   /**< Normal pairwise */
#define TRIPLET_STDP_SLEEP_A2_DROWSY            0.8f   /**< Reduced */
#define TRIPLET_STDP_SLEEP_A2_LIGHT_NREM        0.6f   /**< Further reduced */
#define TRIPLET_STDP_SLEEP_A2_DEEP_NREM         0.5f   /**< Minimal pairwise */
#define TRIPLET_STDP_SLEEP_A2_REM               1.0f   /**< Normal pairwise */

/* Triplet amplitude modulation (A3_plus, A3_minus) */
#define TRIPLET_STDP_SLEEP_A3_AWAKE             1.0f   /**< Normal triplet */
#define TRIPLET_STDP_SLEEP_A3_DROWSY            0.7f   /**< Reduced */
#define TRIPLET_STDP_SLEEP_A3_LIGHT_NREM        0.5f   /**< Suppressed */
#define TRIPLET_STDP_SLEEP_A3_DEEP_NREM         0.3f   /**< Strongly suppressed */
#define TRIPLET_STDP_SLEEP_A3_REM               1.5f   /**< Enhanced (consolidation) */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Sleep-triplet STDP bridge configuration
 */
typedef struct {
    bool enable_tau_fast_modulation;  /**< Enable fast trace modulation */
    bool enable_tau_slow_modulation;  /**< Enable slow trace modulation */
    bool enable_a2_modulation;        /**< Enable pairwise amplitude modulation */
    bool enable_a3_modulation;        /**< Enable triplet amplitude modulation */
    float modulation_strength;        /**< Overall strength (0-1) */
} triplet_stdp_sleep_config_t;

/**
 * @brief Computed sleep effects on triplet STDP
 */
typedef struct {
    /* Time constant modulation */
    float tau_fast_factor;        /**< Multiply tau_plus, tau_minus by this */
    float tau_slow_factor;        /**< Multiply tau_x, tau_y by this */

    /* Amplitude modulation */
    float a2_factor;              /**< Pairwise amplitude scaling */
    float a3_factor;              /**< Triplet amplitude scaling */

    /* State info */
    sleep_state_t current_state;  /**< Current sleep state */
    float sleep_pressure;         /**< Current sleep pressure */
    bool plasticity_enabled;      /**< False during deep offline */

    /* Effective parameters (base * factors) */
    float effective_tau_plus;
    float effective_tau_minus;
    float effective_tau_x;
    float effective_tau_y;
    float effective_A2_plus;
    float effective_A2_minus;
    float effective_A3_plus;
    float effective_A3_minus;
} triplet_stdp_sleep_effects_t;

/**
 * @brief Sleep-triplet STDP integration bridge (opaque)
 */
typedef struct triplet_stdp_sleep_bridge_struct* triplet_stdp_sleep_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default sleep-triplet STDP bridge configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization based on sleep-plasticity research
 * HOW:  Return struct with all modulations enabled
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int triplet_stdp_sleep_default_config(triplet_stdp_sleep_config_t* config);

/**
 * @brief Create sleep-triplet STDP bridge
 *
 * WHAT: Initialize integration between sleep and triplet STDP systems
 * WHY:  Enable sleep-dependent modulation of triplet plasticity
 * HOW:  Allocate structure, link to sleep system
 *
 * @param config Configuration (NULL for defaults)
 * @param sleep_system Sleep/wake system handle
 * @return New bridge or NULL on failure
 */
triplet_stdp_sleep_bridge_t triplet_stdp_sleep_bridge_create(
    const triplet_stdp_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * @brief Destroy sleep-triplet STDP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure, destroy mutex
 *
 * @param bridge Bridge to destroy
 */
void triplet_stdp_sleep_bridge_destroy(triplet_stdp_sleep_bridge_t bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update triplet STDP effects from current sleep state
 *
 * WHAT: Compute how sleep state affects triplet STDP parameters
 * WHY:  Sleep modulation must be computed before applying to synapses
 * HOW:  Query sleep system, compute factors for each parameter
 *
 * @param bridge Sleep bridge
 * @return 0 on success, -1 on error
 */
int triplet_stdp_sleep_update(triplet_stdp_sleep_bridge_t bridge);

/**
 * @brief Get modulated triplet STDP parameters for current sleep state
 *
 * WHAT: Retrieve computed sleep effects
 * WHY:  Apply to synapses during updates
 * HOW:  Copy internal effects structure
 *
 * @param bridge Sleep bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int triplet_stdp_sleep_get_effects(
    const triplet_stdp_sleep_bridge_t bridge,
    triplet_stdp_sleep_effects_t* effects
);

/**
 * @brief Get effective tau_plus for current sleep state
 *
 * @param bridge Sleep bridge
 * @param base_tau_plus Base tau_plus value
 * @return Modulated tau_plus
 */
float triplet_stdp_sleep_get_tau_plus(
    const triplet_stdp_sleep_bridge_t bridge,
    float base_tau_plus
);

/**
 * @brief Get effective tau_minus for current sleep state
 *
 * @param bridge Sleep bridge
 * @param base_tau_minus Base tau_minus value
 * @return Modulated tau_minus
 */
float triplet_stdp_sleep_get_tau_minus(
    const triplet_stdp_sleep_bridge_t bridge,
    float base_tau_minus
);

/**
 * @brief Get effective tau_x for current sleep state
 *
 * @param bridge Sleep bridge
 * @param base_tau_x Base tau_x value
 * @return Modulated tau_x
 */
float triplet_stdp_sleep_get_tau_x(
    const triplet_stdp_sleep_bridge_t bridge,
    float base_tau_x
);

/**
 * @brief Get effective tau_y for current sleep state
 *
 * @param bridge Sleep bridge
 * @param base_tau_y Base tau_y value
 * @return Modulated tau_y
 */
float triplet_stdp_sleep_get_tau_y(
    const triplet_stdp_sleep_bridge_t bridge,
    float base_tau_y
);

/**
 * @brief Apply sleep modulation to synapse
 *
 * WHAT: Update synapse parameters based on current sleep state
 * WHY:  Realize sleep effects on plasticity
 * HOW:  Modify synapse time constants and amplitudes
 *
 * @param bridge Sleep bridge
 * @param synapse Synapse to modulate
 * @return 0 on success, -1 on error
 */
int triplet_stdp_sleep_apply_modulation(
    triplet_stdp_sleep_bridge_t bridge,
    triplet_stdp_synapse_t* synapse
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get fast trace time constant factor for sleep state
 *
 * @param state Sleep state
 * @return Tau modulation factor
 */
float triplet_stdp_sleep_get_tau_fast_factor(sleep_state_t state);

/**
 * @brief Get slow trace time constant factor for sleep state
 *
 * @param state Sleep state
 * @return Tau modulation factor
 */
float triplet_stdp_sleep_get_tau_slow_factor(sleep_state_t state);

/**
 * @brief Get pairwise amplitude factor for sleep state
 *
 * @param state Sleep state
 * @return A2 amplitude factor
 */
float triplet_stdp_sleep_get_a2_factor(sleep_state_t state);

/**
 * @brief Get triplet amplitude factor for sleep state
 *
 * @param state Sleep state
 * @return A3 amplitude factor
 */
float triplet_stdp_sleep_get_a3_factor(sleep_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRIPLET_STDP_SLEEP_BRIDGE_H */
