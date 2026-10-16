/**
 * @file nimcp_lnn_sleep_bridge.h
 * @brief Sleep-LNN Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between sleep/wake system and Liquid Neural Network dynamics
 * WHY:  Sleep states alter continuous-time dynamics and learning in LNNs
 * HOW:  Sleep state modulates time constants (tau), ODE solver dt, and learning rate
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * LIQUID DYNAMICS AND SLEEP:
 * ---------------------------
 * Liquid neural networks model continuous-time neural dynamics with adaptive time
 * constants. Sleep states fundamentally alter the temporal characteristics of neural
 * processing, which should be reflected in liquid dynamics.
 *
 * SLEEP → LNN PATHWAYS:
 * ----------------------
 * 1. Time Constant Modulation During Sleep:
 *    - AWAKE: Normal tau dynamics (fast integration)
 *    - DROWSY: Slightly slower dynamics (tau increases 10%)
 *    - LIGHT_NREM: Moderate slowing (tau increases 30%)
 *    - DEEP_NREM: Maximum slowing (tau increases 50%)
 *    - REM: Partial restoration (tau decreases 20%)
 *    - Biological basis: Reduced firing rates and slower oscillations during sleep
 *    - Reference: Steriade et al. (2001) "Natural waking and sleep states: a view
 *      from inside neocortical neurons"
 *
 * 2. ODE Solver Time Step Adjustment:
 *    - During sleep, neural dynamics slow → can use larger dt for efficiency
 *    - AWAKE: Normal dt
 *    - DROWSY: dt × 1.1 (slightly coarser)
 *    - LIGHT_NREM: dt × 1.3
 *    - DEEP_NREM: dt × 1.5 (coarsest, matches slow-wave)
 *    - REM: dt × 0.9 (finer, captures rapid transitions)
 *    - Biological basis: Matches oscillation frequencies per sleep stage
 *
 * 3. Learning Rate Modulation:
 *    - AWAKE: Full learning rate (online learning)
 *    - DROWSY: Reduced to 60% (transition)
 *    - LIGHT_NREM: Reduced to 40% (consolidation mode)
 *    - DEEP_NREM: Reduced to 30% (minimal plasticity, homeostasis)
 *    - REM: Moderate 50% (selective consolidation)
 *    - Biological basis: Sleep favors replay/consolidation over new learning
 *    - Reference: Tononi & Cirelli (2014) "Sleep and the price of plasticity"
 *
 * 4. State Stability During Sleep:
 *    - Deep sleep increases numerical stability tolerance
 *    - REM allows more variability (supports creativity)
 *    - Biological basis: Delta oscillations provide stable baseline
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    SLEEP-LNN INTEGRATION BRIDGE                            ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   SLEEP STATE      τ Factor   dt Factor   LR Factor   Effect              ║
 * ║   ─────────────────────────────────────────────────────────────────────   ║
 * ║   AWAKE            1.0         1.0         1.0         Normal dynamics    ║
 * ║   DROWSY           1.1         1.1         0.6         Slight slowing     ║
 * ║   LIGHT_NREM       1.3         1.3         0.4         Moderate slowing   ║
 * ║   DEEP_NREM        1.5         1.5         0.3         Maximum slowing    ║
 * ║   REM              0.8         0.9         0.5         Faster, variable   ║
 * ║                                                                            ║
 * ║   TIME CONSTANT MODULATION:                                               ║
 * ║   τ_effective = τ_base × sleep_tau_factor                                 ║
 * ║   → Slower integration during sleep (matches reduced firing rates)        ║
 * ║                                                                            ║
 * ║   ODE TIME STEP MODULATION:                                               ║
 * ║   dt_effective = dt_base × sleep_dt_factor                                ║
 * ║   → Coarser steps during slow-wave sleep (computational efficiency)       ║
 * ║                                                                            ║
 * ║   LEARNING RATE MODULATION:                                               ║
 * ║   lr_effective = lr_base × sleep_lr_factor                                ║
 * ║   → Reduced plasticity during sleep (consolidation, not acquisition)      ║
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

#ifndef NIMCP_LNN_SLEEP_BRIDGE_H
#define NIMCP_LNN_SLEEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "cognitive/nimcp_sleep_wake.h"
#include "lnn/nimcp_lnn_types.h"
#include "utils/thread/nimcp_thread.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Time constant (tau) modulation by sleep state */
#define LNN_SLEEP_TAU_AWAKE       1.0f   /**< Normal tau dynamics */
#define LNN_SLEEP_TAU_DROWSY      1.1f   /**< Slightly slower */
#define LNN_SLEEP_TAU_LIGHT_NREM  1.3f   /**< Moderate slowing */
#define LNN_SLEEP_TAU_DEEP_NREM   1.5f   /**< Maximum slowing (slow-wave) */
#define LNN_SLEEP_TAU_REM         0.8f   /**< Faster (rapid transitions) */

/* ODE solver dt modulation by sleep state */
#define LNN_SLEEP_DT_AWAKE        1.0f   /**< Normal time step */
#define LNN_SLEEP_DT_DROWSY       1.1f   /**< Slightly coarser */
#define LNN_SLEEP_DT_LIGHT_NREM   1.3f   /**< Coarser step */
#define LNN_SLEEP_DT_DEEP_NREM    1.5f   /**< Coarsest (matches delta) */
#define LNN_SLEEP_DT_REM          0.9f   /**< Finer (rapid dynamics) */

/* Learning rate modulation by sleep state */
#define LNN_SLEEP_LR_AWAKE        1.0f   /**< Full online learning */
#define LNN_SLEEP_LR_DROWSY       0.6f   /**< Reduced (transition) */
#define LNN_SLEEP_LR_LIGHT_NREM   0.4f   /**< Consolidation mode */
#define LNN_SLEEP_LR_DEEP_NREM    0.3f   /**< Minimal plasticity */
#define LNN_SLEEP_LR_REM          0.5f   /**< Selective consolidation */

/* ============================================================================
 * Type Definitions
 * ============================================================================ */

/**
 * @brief Configuration for LNN sleep bridge
 *
 * WHAT: Control which sleep effects are enabled for LNN
 * WHY:  Allow selective modulation for different experimental scenarios
 * HOW:  Boolean flags and strength multiplier
 */
typedef struct {
    bool enable_tau_modulation;        /**< Enable time constant modulation */
    bool enable_dt_modulation;         /**< Enable ODE dt modulation */
    bool enable_lr_modulation;         /**< Enable learning rate modulation */
    float modulation_strength;         /**< Overall strength multiplier [0.5-2.0] */
} lnn_sleep_config_t;

/**
 * @brief Sleep effects on LNN dynamics
 *
 * WHAT: Computed modulation factors for current sleep state
 * WHY:  Encapsulate all sleep-induced changes
 * HOW:  Factors multiply base parameters (tau, dt, lr)
 */
typedef struct {
    float tau_factor;                  /**< Multiply tau by this */
    float dt_factor;                   /**< Multiply dt by this */
    float learning_rate_factor;        /**< Multiply LR by this */
    sleep_state_t current_state;       /**< Current sleep state */
    float sleep_pressure;              /**< Current sleep pressure [0-1] */
    bool dynamics_slowed;              /**< True if dynamics slower than awake */
} lnn_sleep_effects_t;

/**
 * @brief Opaque handle to LNN sleep bridge
 *
 * WHAT: Handle to sleep-LNN integration context
 * WHY:  Encapsulation of internal state
 * HOW:  Pointer to internal structure
 */
typedef struct lnn_sleep_bridge_struct* lnn_sleep_bridge_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Get default LNN sleep bridge configuration
 *
 * WHAT: Provide sensible defaults for LNN sleep integration
 * WHY:  Easy initialization with biological defaults
 * HOW:  All modulations enabled, strength = 1.0
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int lnn_sleep_default_config(lnn_sleep_config_t* config);

/**
 * @brief Create LNN sleep bridge
 *
 * WHAT: Initialize bidirectional sleep-LNN integration
 * WHY:  Enable realistic sleep modulation of liquid dynamics
 * HOW:  Allocate structure, link sleep system, register callbacks
 *
 * @param config Configuration (NULL for defaults)
 * @param sleep_system Sleep/wake system handle
 * @return Bridge handle or NULL on failure
 */
lnn_sleep_bridge_t lnn_sleep_bridge_create(
    const lnn_sleep_config_t* config,
    sleep_system_t sleep_system
);

/**
 * @brief Destroy LNN sleep bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Unregister callbacks, free structure
 *
 * @param bridge Bridge to destroy (safe to call with NULL)
 */
void lnn_sleep_bridge_destroy(lnn_sleep_bridge_t bridge);

/**
 * @brief Update LNN sleep effects
 *
 * WHAT: Query sleep system and recompute modulation factors
 * WHY:  Keep effects synchronized with sleep state
 * HOW:  Query current state, map to tau/dt/lr factors
 *
 * @param bridge LNN sleep bridge
 * @return 0 on success, -1 on error
 */
int lnn_sleep_update(lnn_sleep_bridge_t bridge);

/**
 * @brief Get current sleep effects
 *
 * WHAT: Retrieve computed modulation factors
 * WHY:  LNN layers need these to adjust dynamics
 * HOW:  Copy effects structure
 *
 * @param bridge LNN sleep bridge
 * @param effects Output effects structure
 * @return 0 on success, -1 on error
 */
int lnn_sleep_get_effects(const lnn_sleep_bridge_t bridge, lnn_sleep_effects_t* effects);

/**
 * @brief Get time constant modulation factor
 *
 * WHAT: Get tau multiplier for current sleep state
 * WHY:  Convenience function for direct tau adjustment
 * HOW:  Return cached tau_factor
 *
 * @param bridge LNN sleep bridge
 * @return tau factor [0.8-1.5] (1.0 if bridge is NULL)
 */
float lnn_sleep_get_tau_factor(const lnn_sleep_bridge_t bridge);

/**
 * @brief Get ODE dt modulation factor
 *
 * WHAT: Get dt multiplier for current sleep state
 * WHY:  Convenience function for ODE step adjustment
 * HOW:  Return cached dt_factor
 *
 * @param bridge LNN sleep bridge
 * @return dt factor [0.9-1.5] (1.0 if bridge is NULL)
 */
float lnn_sleep_get_dt_factor(const lnn_sleep_bridge_t bridge);

/**
 * @brief Get learning rate modulation factor
 *
 * WHAT: Get lr multiplier for current sleep state
 * WHY:  Convenience function for learning rate adjustment
 * HOW:  Return cached learning_rate_factor
 *
 * @param bridge LNN sleep bridge
 * @return lr factor [0.3-1.0] (1.0 if bridge is NULL)
 */
float lnn_sleep_get_lr_factor(const lnn_sleep_bridge_t bridge);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get tau factor for specific sleep state
 *
 * WHAT: Map sleep state to time constant modulation
 * WHY:  Stateless helper for tau computation
 * HOW:  Switch on sleep state enum
 *
 * @param state Sleep state
 * @return tau factor for that state
 */
float lnn_sleep_tau_for_state(sleep_state_t state);

/**
 * @brief Get dt factor for specific sleep state
 *
 * WHAT: Map sleep state to ODE dt modulation
 * WHY:  Stateless helper for dt computation
 * HOW:  Switch on sleep state enum
 *
 * @param state Sleep state
 * @return dt factor for that state
 */
float lnn_sleep_dt_for_state(sleep_state_t state);

/**
 * @brief Get lr factor for specific sleep state
 *
 * WHAT: Map sleep state to learning rate modulation
 * WHY:  Stateless helper for lr computation
 * HOW:  Switch on sleep state enum
 *
 * @param state Sleep state
 * @return lr factor for that state
 */
float lnn_sleep_lr_for_state(sleep_state_t state);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed LNN sleep signals
 * HOW:  Register with bio_router using BIO_MODULE_LNN_SLEEP
 *
 * @param bridge LNN sleep bridge
 * @return 0 on success, -1 on error
 */
int lnn_sleep_bridge_connect_bio_async(lnn_sleep_bridge_t bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge LNN sleep bridge
 * @return 0 on success, -1 on error
 */
int lnn_sleep_bridge_disconnect_bio_async(lnn_sleep_bridge_t bridge);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Query bio-async connection status
 * WHY:  Allow conditional bio-async usage
 * HOW:  Return bio_async_enabled flag
 *
 * @param bridge LNN sleep bridge
 * @return true if connected
 */
bool lnn_sleep_bridge_is_bio_async_connected(const lnn_sleep_bridge_t bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_SLEEP_BRIDGE_H */
